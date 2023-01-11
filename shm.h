
#define SHM_KEY 6969


#define BUFFERS_SIZE 100
#define TXT_SZ 134

/* Struct used for a single buffer block in the circular buffer
 * bytes_to_count stores the number of elements in string_data
 * string_data is a non-null terminated string of the input data
*/
struct buffer {
 	int bytes_to_count;
 	char string_data[TXT_SZ];\
 	int total_byte_count;
};

/* Struct used for passing metadata between producer and consumer
 */
struct metadata {
	int file_byte_count;
};

