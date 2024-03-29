// The little schedule library I wrote for myself

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#ifdef _WIN32
	#include <malloc.h>
	#define strcasecmp _stricmp
	#define getcwd _getcwd
//	#define _CRT_NONSTDC_NO_WARNINGS
#else
	#include <alloca.h>
	#include <strings.h>
	#include <unistd.h>
	#include <float.h>
#endif

// On preemptible schedules we can finish at a fraction
#define TIME double
//#define TIME_MAX DBL_MAX
// 0 means not set, starts at 1 for ids
#define MACHINE unsigned int
#define INDEX size_t

typedef struct {
	INDEX j;
	TIME prdw[4];
} Job;

#define P 0
#define R 1
#define D 2
#define W 3

// Collection of jobs, possibly unfeasible ordering
typedef struct {
	unsigned char which_set;
	size_t length;
	Job* buffer;
} Input;

int require_set(Input* input, unsigned char data) {
	if ((data & input->which_set) != data) {
		return 1;
	}

	return 0;
}

typedef struct {
	TIME start;
	TIME end;
	INDEX job_j;
	MACHINE m_id;
} ScheduledJob;

typedef struct {
	Input* input;
	ScheduledJob* schedule;

	// Length of the schedule is input->capacity in regular case, but in preemption the schedule can have more entries than jobs
	size_t length;
} Schedule;


int length_of_number(unsigned int number) {
	if (number == 0) {
		return 1;
	} else {
		return (int) floor(log10(number)) + 1;
	}
}

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define J_TITLE "j"
#define S_TITLE "s_j"
#define C_TITLE "c_j"
#define M_TITLE "M"

void schedule_print(Schedule* schedule) {
	size_t largest_j = 0, largest_s = 0, largest_c = 0, largest_m = 0;
	for (size_t i = 0; i < schedule->length; i++) {
		ScheduledJob* schedule_data = schedule->schedule + i;
		largest_j = MAX(schedule_data->job_j, largest_j);
		largest_s = MAX((size_t) schedule_data->start, largest_s);
		largest_c = MAX((size_t) schedule_data->end, largest_c);
		largest_m = MAX((size_t) schedule_data->m_id, largest_m);
	}
	int num_j = MAX((size_t) length_of_number((unsigned int) largest_j), strlen(J_TITLE));
	int num_s = MAX((size_t) length_of_number((unsigned int) largest_s), strlen(S_TITLE));
	int num_c = MAX((size_t) length_of_number((unsigned int) largest_c), strlen(C_TITLE));
	int num_m = MAX((size_t) length_of_number((unsigned int) largest_m), strlen(M_TITLE));

	printf("%*s|%*s|%*s|%*s\n", num_j, J_TITLE, num_s, S_TITLE, num_c, C_TITLE, num_m, M_TITLE);
	for (int i = 0; i < num_j + num_s + num_c + num_m + 3; i++) {
		printf("_");
	}
	printf("\n");

	for (size_t i = 0; i < schedule->length; i++) {
		ScheduledJob* schedule_data = &schedule->schedule[i];
		printf("%*zd|%*lu|%*lu|%*u\n", num_j, schedule_data->job_j, num_s, (unsigned long) ceil(schedule_data->start), num_c,
		       (unsigned long) ceil(schedule_data->end),
		       num_m, schedule_data->m_id);
	}
}
// num digits of MAX_ULONG_LEN 18446744073709551615
#define MAX_ULONG_LEN 20
//                       processing time ", " ready time     ", " deadline         + ", " + weight    \r \n  \0
#define LINE_BUFFER_SIZE (MAX_ULONG_LEN + 2 + MAX_ULONG_LEN + 2 + MAX_ULONG_LEN + 2 + MAX_ULONG_LEN + 1 + 1 + 1)
#define NO_PROPERTY 0xFF

//https://stackoverflow.com/questions/11793689/read-the-entire-contents-of-a-file-to-c-char-including-new-lines
int read_input_from_file(const char* path, Input* input) {
	size_t buffer_size = 8;
	input->buffer = malloc(buffer_size * sizeof(Job));

	char line_buffer[LINE_BUFFER_SIZE];
	size_t num_rows = 0;

	FILE* file = NULL;
	if (!(file = fopen(path, "rb"))) {
		char cwd[512];
		getcwd(cwd, sizeof(cwd));
		fprintf(stderr, "Couldn't open file '%s' in cwd '%s'; FILE: '%s'\n", path, cwd, __FILE__);
		goto error_cleanup;
	}
	// e.g. {P, R, NP,NP} corresponds to header p,r; 5th value is sentinel to stop loop over this array
	unsigned char order_prdw[5] = {NO_PROPERTY, NO_PROPERTY, NO_PROPERTY, NO_PROPERTY, NO_PROPERTY};
	// Read header row
	if (fgets(line_buffer, LINE_BUFFER_SIZE, file) != NULL) {
		// Separate indices as we
		unsigned int char_index = 0;
		unsigned char property_index = 0;
		while (line_buffer[char_index] != '\0') {
			switch (line_buffer[char_index]) {
				case 'p':
					order_prdw[property_index] = P;
					break;
				case 'r':
					order_prdw[property_index] = R;
					break;
				case 'd':
					order_prdw[property_index] = D;
					break;
				case 'w':
					order_prdw[property_index] = W;
					break;
				case '\r':
				case '\n':
				case '\t':
				case ',':
					property_index--;
					break;
				default:
					goto no_header_row;
			}
			char_index++;
			property_index++;
		}
	} else {
		goto no_header_row;
	}
	for (int i = 0; i < 4; i++) {
		unsigned char pos = order_prdw[i];
		if (pos != NO_PROPERTY) {
			input->which_set |= 1 << pos;
		} else {
			break;
		}
	}
	// https://cplusplus.com/reference/cstdio/fgets/
	while (fgets(line_buffer, LINE_BUFFER_SIZE, file) != NULL) {
		if (num_rows >= buffer_size) {
			buffer_size *= 2;
			if (!realloc(input->buffer, buffer_size)) {
				perror("OOM");
				goto error_cleanup;
			}
		}

		char* parse_next = line_buffer;
		char* endptr;
		Job job = {
			.j = num_rows + 1, // including header + 1
			.prdw = {0, 0, ULONG_MAX, 0}
		};
		unsigned char* property = order_prdw;
		while (*property != NO_PROPERTY) {
			// We get property number, which corresponds to index to use for saving in prdw array
			errno = 0;
			TIME value = (double) strtoull(parse_next, &endptr, 10);
			if (errno) {
				// Error in conversion
				goto error;
			} else if (*parse_next != '\0' && parse_next == endptr) {
				// Hasn't moved (invalid character)

				// Make sure it is not `inf`
				char* comma = strchr(parse_next, ',');
				if (comma != NULL) {
					*comma = '\0'; // Found comma, no need to restore it as we'll jump past it
				} else {
					parse_next[3] = '\0'; // only interested in 3 characters (gets rid of \r\n etc.)
				}
				if (strcasecmp(parse_next, "inf") == 0) {
					value = ULONG_MAX;
					endptr = comma; // so next time we start after the comma we found
					goto success;
				}

				goto error;
			} else {
				goto success;
			}
error:
			fprintf(stderr, "Data file %s invalid on line %zu\n", path, num_rows + 1);
			goto error_cleanup;
success:
			job.prdw[*property] = value;
			parse_next = endptr + 1; // + 1 to get past the comma
			property++;
		}

		*(input->buffer + num_rows) = job;
		num_rows++;
	}
	if (num_rows == 0) {
		fprintf(stderr, "Not enough values in file\n");
		goto error_cleanup;
	}
	input->length = num_rows;
	fclose(file);
	return 0;

no_header_row:
	fprintf(stderr, "No header row in file: %s\n", path);
	// also needs error cleanup

error_cleanup:
	free(input->buffer);
	input->buffer = NULL;
	if (file != NULL) {
		fclose(file);
	}
	return 1;
}

typedef int (* COMPARE_FUNC)(const void*, const void*);

#define ERROR_WITH_HELP 789

void schedule_new(Schedule* inout_schedule, Input* in_input) {
	inout_schedule->input = in_input;
	inout_schedule->schedule = malloc(in_input->length * sizeof(Job));
	TIME starting_time = 0;
	for (unsigned int i = 0; i < in_input->length; i++) {
		Job* job = &in_input->buffer[i];
		starting_time = job->prdw[R] > starting_time ? job->prdw[R] : starting_time;
		ScheduledJob schedule_data = {
			.start = starting_time,
			.end = starting_time + job->prdw[P],
		};
		starting_time = schedule_data.end;
		inout_schedule->schedule[i] = schedule_data;
	}
}

typedef TIME (* GET_INT)(const void*);

int max_time(void* list, size_t elem_size, size_t len, GET_INT get_int, TIME* out) {
	TIME max = 0;
	size_t i;
	for (i = 0; i < len; i++) {
		const void* elem = (void*) ((char*) list + i * elem_size);
		max = MAX(max, get_int(elem));
	}
	if (i == 0) {
		return -1;
	} else {
		*out = max;
		return 0;
	}
}

int sum_time(void* list, size_t elem_size, size_t len, GET_INT get_int, TIME* out) {
	TIME sum = 0;
	size_t i;
	for (i = 0; i < len; i++) {
		const void* elem = (void*) ((char*) list + i * elem_size);
		sum += get_int(elem);
	}
	if (i == 0) {
		return -1;
	} else {
		*out = sum;
		return 0;
	}
}
