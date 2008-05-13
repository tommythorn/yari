#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <assert.h>
#include <limits.h>

/// exit codes
#define EXIT_OPTION 1
#define EXIT_INFILE 2
#define EXIT_OUTFILE 3

/// size of input buffer
#define BUFFER_SIZE 4096

// program_invocation_short_name
char *arg0;

/// print help message
void print_help()
{
	fprintf(stderr, "wrapper [-o <outfile>] <infiles>\n"
		"  Generates a file with the input files wrapped into a C array.\n\n");
}

/// initialize the output file
FILE *init_outfile(char *outfile, int classnumber) 
{
	FILE *out = stdout;
	if (strcmp(outfile, "-") != 0) {
		out = fopen(outfile, "w");

		if (!out) {
			char *err = strerror(errno);
			fprintf(stderr, "%s: Error opening output file: %s: %s\n", 
					arg0, outfile, err);
			return NULL;
		}
	}

	fprintf(out, "#include \"embedded_classes.h\"\n\n");

	// print number of embedded classes
	fprintf(out, "int embedded_class_number = %d;\n\n", classnumber);

	// print embedded classes struct
	fprintf(out, "struct embedded_classinfo embedded_classes[%d] = {\n", 
				 classnumber);

	return out;
}

/// close the output file
int finalize_outfile(FILE *out)
{
	fprintf(out, "};\n\n");

	if (fclose(out)) {
		char *err = strerror(errno);
		fprintf(stderr, "%s: Error closing output file: %s\n", arg0, err);
		return -1;
	}

	return 0;
}

/// open input file for reading
FILE *init_input(char *infile)
{
	if (strcmp(infile, "-") != 0) {
		FILE *result = fopen(infile, "r");

		if (!result) {
			char *err = strerror(errno);
			fprintf(stderr, "%s: Error opening input file: %s: %s\n", 
					arg0, infile, err);
			return NULL;
		}

		return result;
	}

	return stdin;
}

/// close an intput file
int close_infile(FILE *in, char *infile)
{
	if (fclose(in)) {
		char *err = strerror(errno);
		fprintf(stderr, "%s: Error closing intput file: %s: %s\n", 
				arg0, infile, err);
		return -1;
	}

	return 0;
}

/// remove special characters from filenames
char *escapename(char *filename)
{
	char *result = malloc(strlen(filename) + 1);

	if (result) {
		char *c = result;
		while (*filename) {
			if (isalnum(*filename) || *filename == '.' || *filename == '/' || *filename == '$')
				*c++ = *filename;
			else
				*c++ = '_';
			++filename;	
		}

		*c++ = '\0';
	}

	return result;
}

/// print binary data
int print_escaped(int count, char *buf, int pos, FILE *out)
{
	int i;
	for (i = 0; i < count; ++i) {
		int c;
		if (pos >= 68) {
			fprintf(out, "\"\n\t\"");
			pos = 0;
		}

		pos += fprintf(out, "\\%03hho", *buf++);
	}

	return pos;
}

/// process an input file and generate a static C array
int process_file(char *infile, FILE *out) 
{
	// open input file
	FILE *in = init_input(infile);
	char buf[BUFFER_SIZE];
	char *base_name = NULL;
	char *dir_name = NULL;
	char *escape_name = NULL; 
	char *escape_fullname = NULL;
	char *escape_pathname = NULL;
	int size = 0, c = 0, pos = INT_MAX;

	// return error code
	if (!in)
		return -1;

	base_name = basename(infile);
	assert(base_name && "basename returned NULL.\n");

	escape_name = escapename(base_name);
	if (!escape_name)
		return -1;

	escape_fullname = escapename(infile);
	if (!escape_fullname)
		return -1;

	dir_name = dirname(infile);
	assert(base_name && "dirname returned NULL.\n");

	escape_pathname = escapename(dir_name);
	if (!escape_pathname)
		return -1;

	// print C code
	fprintf(out, "\t/* classfile: %s */\n", base_name);
	fprintf(out, "\t{path:\"%s\",\n\t "
				 "fullname:\"%s\",\n\t "
				 "name:\"%s\",\n\t "
				 "data: \"",
				 escape_pathname, escape_fullname, escape_name, 4);

	// print binary data
	while (c = fread(buf, sizeof(char), BUFFER_SIZE, in)) {
		size += c;
		pos = print_escaped(c, buf, pos, out);
	}

	if (pos != 0)
		fprintf(out, "\"\n");

	fprintf(out, "\t, size: %d},\n", size);

	// check for errors
	if (!feof(in) || ferror(in)) {
		char *err = strerror(errno);
		fprintf(stderr, "%s: Error reading input file: %s: %s\n", 
				arg0, infile, err);
		return -1;
	}

	// close file
	if (!close_infile(in, infile));
		return -1;

	// free strings
	free(escape_name);
	free(escape_pathname);

	return 0;
}

int main(int argc, char **argv) 
{
	int c;
	char *outfile = "-";
	FILE *out = NULL;

        arg0 = argv[0];

	// read options
	while ((c = getopt(argc, argv, "o:")) != -1) {
		switch (c) {
			case 'o':
				outfile = optarg;
				break;
			case 'h':
				print_help();
				return EXIT_OPTION;
				break;
			default:
				print_help();
				fprintf(stderr, "Invalid option: %c\n", c);
				return EXIT_OPTION;
		}
	}

	// check for input files
	if (optind >= argc) {
		print_help();
		fprintf(stderr, "No input specified.\n");
		return EXIT_OPTION;
	}

	// initialize the output stream
	if (!(out = init_outfile(outfile, argc - optind)))
		return EXIT_OUTFILE;

	// process each class file
	for(c = optind; c < argc; ++c) {
		if (!process_file(argv[c], out))
			return EXIT_INFILE;
	}

	// close the output stream
	if (finalize_outfile(out))
		return EXIT_OUTFILE;

	return 0;
}
