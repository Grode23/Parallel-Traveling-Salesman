#include "mpi.h"
#include <time.h>

#include "headers/matrix_generator.h"
#include "headers/common_functions.h"
#include "headers/arguments.h"

double begin = 0;
/*
* parse_opt is a function required by Argp library
* Every case is a different argument
*/
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;
	switch (key) {
	case 'r':
		arguments->mode = READ_MODE;
		break;
	case 'w':
		arguments->mode = WRITE_MODE;
		break;
	case 's':
		arguments->size = atoi(arg);
		break;
	case 'f':
		arguments->file_name = arg;
	case ARGP_KEY_ARG: return 0;
	default: return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/*
* Recursion is the main function that gets accessed for almost every node
* (except for first and second node)
*/
void recursion(
    int size,
    int adj[size][size],
    int curr_bound,
    int curr_weight,
    int level,
    int curr_path[size + 1],
    int visited[size],
    int** first_mins, int** second_mins,
    int rank) {

	// If every node has been visited
	if (level == size) {

		int curr_res = curr_weight + adj[curr_path[level - 1]][curr_path[0]];

		// If my current result is less than the best so far
		// Copy current into best (result and path too)
		if (curr_res < final_res) {
			copy_to_final(size, curr_path, final_path);
			final_res = curr_res;
		}

		return;
	}

	// Go through every node
	for (int i = 1; i < size; i++) {

		//if (rank == 1)
		//	printf("%d\n", curr_path[1] );

		// Check if the node has been visited
		if (visited[i] == 0) {

			// if(rank == 1)
			// 	printf("%d\n",curr_path[1] );


			// Change variables due to the next visiting
			int temp = curr_bound;
			curr_weight += adj[curr_path[level - 1]][i];
			curr_bound -= ((*(*second_mins + curr_path[level - 1]) + * (*first_mins + i)) / 2);

			// If current result is less than the bound
			if (curr_bound + curr_weight < final_res) {
				curr_path[level] = i;
				visited[i] = 1;
				recursion(size, adj, curr_bound, curr_weight, level + 1, curr_path, visited, first_mins, second_mins, rank);
			}

			// Restore variables back to normal
			// Before the current node had been visited
			curr_bound = temp;
			curr_weight -= adj[curr_path[level - 1]][i];
			visited[i] = 0;

			// If uncommented, the current path is not always correct during execution
			// Because it contains previous paths (before backtracking)
			// The outcome is always the same
			// Every other variable is necessary to change because their values are being compared
			// I keep it for the better understanding of the program
			//curr_path[level] = -1;
		}
	}
}


void second_node(
    int size,
    int adj[size][size],
    int curr_bound,
    int curr_path[size + 1],
    int visited[size],
    int recv_size,
    int recv_second[recv_size],
    int** first_mins, int** second_mins,
    int rank) {

	for (int i = 0; i < recv_size; i++) {
		if (recv_second[i] != -1) {
	//MPI_Win_create(sharedbuffer, 5, sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &win);

			double start = MPI_Wtime();

			int temp = curr_bound;
			curr_bound -= ((*(*second_mins + curr_path[0]) + * (*first_mins + i)) / 2);

			curr_path[1] = recv_second[i];
			visited[recv_second[i]] = 1;

			//if(rank == 1)
			//printf("In second nodes %d\n", curr_path[1] );

			recursion(size, adj, curr_bound, adj[curr_path[0]][recv_second[i]], 2, curr_path, visited, first_mins, second_mins, rank);

			curr_bound = temp;
			memset(visited, 0, sizeof(int)*size);
			visited[0] = 1;

			// if (rank == 1)
				printf("Second: %d, time: %f\n", recv_second[i], MPI_Wtime() - start );


		}

		//All these happens because I have to communicate and change bounds now and then
		int finals[4];

		MPI_Barrier(MPI_COMM_WORLD);

		MPI_Allgather(&final_res, 1, MPI_INT, finals, 1, MPI_INT, MPI_COMM_WORLD);

		for (int i = 0; i < 4; i++) {
			if (finals[i] < final_res) {
				final_res = finals[i];
			}
		}

	}

}

void first_node(
    int size,
    int adj[size][size],
    int numtasks,
    int rank,
    int** first_mins, int** second_mins) {

	int curr_path[size + 1];
	memset(curr_path, -1, sizeof(curr_path));
	curr_path[0] = 0;

	int visited[size];
	memset(visited, 0, sizeof(visited));
	visited[0] = 1;

	int init_bound = 0;
	init_bound = *(*first_mins);

	for (int i = 1; i < size; i++) {
		init_bound += *(*first_mins + i) + *(*second_mins + i);
	}

	if (init_bound == 1) {
		init_bound = init_bound / 2 + 1;
	} else {
		init_bound = init_bound / 2;
	}



	//ΜΠΟΡΕΙ ΝΑ ΦΥΓΕΙ ΤΟ ΙΦ ΚΑΙ ΝΑ ΜΕΙΝΕΙ ΜΟΝΟ ΤΟ ELSE ΓΙΑ ΠΙΟ ΚΑΛΑ, ΑΛΛΑ ΔΕΝ ΜΕ ΚΑΙΕΙ ΤΩΡΑ
	int recv_size;
	if ((size - 1) % numtasks == 0) {
		recv_size = size / numtasks;
	} else {
		recv_size = ((size - 1) / numtasks) + 1;
	}

	//Second nodes that every proccessor will get
	// ***SPOILER ALERT*** some of them might be -1
	int recv_second[recv_size];

	//All second nodes (with the -1s too)
	int seconds[numtasks * recv_size];

	if (rank == 0) {
		//How many real seconds nodes
		int how_many_each[numtasks];

		//How many second nodes are the minimum
		int baseline_of_seconds = size / numtasks;

		// Minus 1, because I don't care about node number 0 (it is the root node)
		int diafora = (size - 1) % numtasks;

		// Calculate how_many_each
		for (int i = 0; i < numtasks; i++) {
			how_many_each[i] = baseline_of_seconds;
			if (diafora > 0) {
				how_many_each[i]++;
				diafora--;
			}

		}

		int second_node = 1;

		//Iterate as many times as the number of machines
		for (int i = 0; i < numtasks; i++) {

			// Iterate as many times as the biggest amount of second nodes on one machine
			for (int j = 0; j < recv_size; j++) {
				if (how_many_each[i] != 0) {
					how_many_each[i]--;
					seconds[(i * recv_size) + j] = second_node++;
				} else {
					seconds[(i * recv_size) + j] = -1;
				}
			}

		}

	}

	//Share all possible second nodes to each processor
	MPI_Scatter(&seconds, recv_size, MPI_INT, recv_second, recv_size, MPI_INT, 0, MPI_COMM_WORLD);

	int curr_bound = init_bound;


	//if (rank == 1) {
	for (int i = 0; i < recv_size; i++) {
		printf("In first node %d, rank: %d\n", recv_second[i], rank );
	}
	printf("\n");
	//}

	second_node(size, adj, curr_bound, curr_path, visited, recv_size, recv_second, first_mins, second_mins, rank);

	//printf("Rank %d, time %f\n",rank, MPI_Wtime() - begin );

}

int main(int argc, char *argv[]) {

	int rank, numtasks;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	struct arguments arguments;

	arguments.size = SIZE;
	arguments.mode = WRITE_MODE;
	arguments.file_name = "example-arrays/file01.txt";

	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	//I need this to create the adj out of the scope of the below if statements
	if (arguments.mode == READ_MODE) {
		arguments.size = get_size_of_matrix(arguments.file_name);
	}

	int adj[arguments.size][arguments.size];

	if (arguments.mode == WRITE_MODE) {
		generator(arguments.size, adj, 50, 99);
		write_to_file(arguments.size, adj, arguments.file_name);
	} else {
		read_from_file(arguments.size, adj, arguments.file_name);
	}

	// I add one more because of a corrupted size vs. prev_size error. Probably needs it to store \0 byte at the end
	final_path = (int*) malloc(arguments.size * sizeof(int) + 1);

	// Check if the memory has been successfully
	// allocated by malloc or not
	if (final_path == NULL) {
		printf("Memory not allocated.\n");
		exit(0);
	}

	//Starting time of solution
	begin = MPI_Wtime();

	//Get first_min and second_min as two arays instead of calling them each time
	int *first_mins = malloc(arguments.size * sizeof(int));
	int *second_mins = malloc(arguments.size * sizeof(int));
	find_mins(arguments.size, &first_mins, &second_mins, adj);

	// This is the only function of the solution in main. Everything else is into this
	first_node(arguments.size, adj, numtasks, rank, &first_mins, &second_mins);

	int finals[numtasks];

	MPI_Gather(&final_res, 1, MPI_INT, finals, 1, MPI_INT, 0, MPI_COMM_WORLD);

	// Index of the rank with the best result
	int index = 0;

	// Find the minimum of each rank's minimum
	if (rank == 0)
	{
		for (int i = 1; i < numtasks; i++) {
			if (finals[i] < final_res) {
				final_res = finals[i];
				index = i;
			}
		}
	}

	MPI_Barrier(MPI_COMM_WORLD);

	// Send index to everyone so they know who must send its path
	MPI_Bcast(&index, 1, MPI_INT, 0, MPI_COMM_WORLD);


	// If the best is master, don't send/receive anything
	if (index != 0) {

		int tag = 1;

		// If this rank has the minimum, then the path
		if (rank == index) {
			MPI_Send(final_path, arguments.size + 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
		} else if (rank == 0) {
			MPI_Status Status;
			MPI_Recv(final_path, arguments.size + 1, MPI_INT, index, tag, MPI_COMM_WORLD, &Status);
		}

	}

	// Display minimum cost and the path of it
	if (rank == 0) {

		double end = MPI_Wtime();
		double time_spent = (double)(end - begin);

		for (int i = 0; i < arguments.size + 1; i++) {
			printf("%d ", final_path[i] );
		}
		printf("\n");

		printf("%d\n",final_res );

		printf("%d %d %f\n", numtasks, arguments.size, time_spent);

	}

	MPI_Finalize();


	return 0;
}