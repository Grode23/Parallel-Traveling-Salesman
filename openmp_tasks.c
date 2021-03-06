#include <omp.h>

#include "headers/matrix_generator.h"
#include "headers/common_functions.h"
#include "headers/arguments.h"

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
		break;
	case 't':
		arguments->num_of_threads = atoi(arg);
		break;
	case 'm':
		arguments->minimum = atoi(arg);
		break;
	case 'M':
		arguments->maximum = atoi(arg);
		break;
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
    int** first_mins, int** second_mins) {

	// If every node has been visited
	if (level == size) {

		int curr_res = curr_weight + adj[curr_path[level - 1]][curr_path[0]];

		#pragma omp critical
		{
			// If my current result is less than the best so far
			// Copy current into best (result and path too)
			if (curr_res < final_res) {
				copy_to_final(size, curr_path, final_path);
				final_res = curr_res;
			}
		}

		return;
	}

	// Go through every node
	for (int i = 1; i < size; i++) {

		// Check if the node has been visited
		if (visited[i] == 0) {

			// Change variables due to the next visiting
			int temp = curr_bound;
			curr_weight += adj[curr_path[level - 1]][i];
			curr_bound -= ((*(*second_mins + curr_path[level - 1]) + * (*first_mins + i)) / 2);

			// If current result is less than the bound
			if (curr_bound + curr_weight < final_res) {
				curr_path[level] = i;
				visited[i] = 1;
				recursion(size, adj, curr_bound, curr_weight, level + 1, curr_path, visited, first_mins, second_mins);
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
			// curr_path[level] = -1;
		}
	}
}

void second_node(
    int size,
    int adj[size][size],
    int curr_bound,
    int curr_path[size + 1],
    int visited[size],
    int j,
    int** first_mins, int** second_mins) {

	curr_bound -= ((*(*first_mins) + * (*first_mins + j)) / 2);
	curr_path[1] = j;
	visited[j] = 1;

	recursion(size, adj, curr_bound, adj[curr_path[0]][j], 2, curr_path, visited, first_mins, second_mins);
}

void first_node(int size, int adj[size][size], int** first_mins, int** second_mins, int num_of_threads) {

	int curr_bound = 0;
	curr_bound = *(*first_mins);

	for (int i = 1; i < size; i++) {
		curr_bound += *(*first_mins + i) + *(*second_mins + i);
	}

	curr_bound = curr_bound / 2;

	omp_set_num_threads(num_of_threads);
	#pragma omp parallel
	{

		#pragma omp single nowait
		{
			for (int j = 1; j < size; j++) {

				#pragma omp task firstprivate(j)
				{
					//Declare curr_path and set it to start from 0 node
					int curr_path[size + 1];
					memset(curr_path, -1, sizeof(curr_path));
					curr_path[0] = 0;

					//Declare visited nodes and set it to have visited
					int visited[size];
					memset(visited, 0, sizeof(visited));
					visited[0] = 1;

					second_node(size, adj, curr_bound, curr_path, visited, j, first_mins, second_mins);
				}
			}
		}
	}
}

int main(int argc, char *argv[]) {

	struct arguments arguments;

	// Default values for arguments
	// In case some of them are not initialized by user's arguments
	arguments.size = SIZE;
	arguments.mode = WRITE_MODE;
	arguments.file_name = "example-arrays/file01.txt";
	arguments.num_of_threads = 8;
	arguments.minimum = 50;
	arguments.maximum = 100;

	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	//I need this to create the adj out of the scope of the below if statements
	if (arguments.mode == READ_MODE) {
		arguments.size = get_size_of_matrix(arguments.file_name);
	}

	int adj[arguments.size][arguments.size];

	// Fill the array of distances
	if (arguments.mode == WRITE_MODE) {
		generator(arguments.size, adj, arguments.minimum, arguments.maximum);
		write_to_file(arguments.size, adj, arguments.file_name);
	} else {
		read_from_file(arguments.size, adj, arguments.file_name);
	}

	final_path = (int *)malloc(arguments.size * sizeof(int));

	// Check if the memory has been successfully
	// allocated by malloc or not
	if (final_path == NULL) {
		printf("Memory not allocated.\n");
		exit(0);
	}

	//Starting time of solution
	double start_time = omp_get_wtime();

	//Get first_min and second_min as two arrays instead of calling them every time I need them
	int *first_mins = malloc(arguments.size * sizeof(int));
	int *second_mins = malloc(arguments.size * sizeof(int));
	find_mins(arguments.size, &first_mins, &second_mins, adj);

	// Start the actual algorithm
	first_node(arguments.size, adj, &first_mins, &second_mins, arguments.num_of_threads);

	// Finishing time of solution
	double final_time = omp_get_wtime() - start_time;

	// for(int i = 0; i < arguments.size; i++){
	// 	printf("%d ",final_path[i] );
	// }
	// printf("\n" );
	// printf("%d\n",final_res );

	// Print result so I can access them through the bash script
	printf("%f", final_time);

	return 0;
}
