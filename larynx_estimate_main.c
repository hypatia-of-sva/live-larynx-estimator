

/*
 * Binary Mode for Pipes (Cross-Platform)
 * 
 * Windows pipes default to text mode, which corrupts binary data by
 * translating LF <-> CRLF. Unix pipes are always binary.
 * 
 * NOTE: PowerShell corrupts binary pipes regardless of this setting.
 *       Use cmd.exe for piped commands on Windows.
 */
#ifdef _WIN32
    #include <fcntl.h>
    #include <io.h>
    #define SET_BINARY_MODE(fp) _setmode(_fileno(fp), _O_BINARY)
#else
    #define SET_BINARY_MODE(fp) ((void)0)
#endif

#include "alad.h"
#include "snd.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <gsl/gsl_fit.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_linalg.h>
#define TIME_PER_ROUND_SECONDS 3

/*
void alinfo(void) {
    printf("General AL info:\n");
    const char* vendor = alGetString(AL_VENDOR);
    const char* ver = alGetString(AL_VERSION);
    const char* rend = alGetString(AL_RENDERER);
    const char* exts = alGetString(AL_EXTENSIONS);
    printf("Vendor: %s\nVersion: %s\nRenderer: %s\nExtensions: %s\n", vendor,ver, rend, exts);
}
*/



volatile int is_interrupted = 0;
void interrupted(int value) {
    is_interrupted = 1;
}
void die(const char* str) {
	fprintf(stderr, str);
	exit(EXIT_FAILURE);
}
char** split(const char* str, size_t len, char sep, int* out_num_strings) {
  assert(out_num_strings != NULL);
  out_num_strings[0] = 1;
  bool in_sep = (str[0] == sep);
  for(int i = 0; i < len; i++) {
    if(str[i] == sep && !in_sep && i != len-1) {
      out_num_strings[0]++;
      in_sep = true;
    } else if (str[i] != sep) {
      in_sep = false;
    }
  }

  char** split_strings = calloc(out_num_strings[0], sizeof(char*));
  int str_idx = 0;
  in_sep = (str[0] == sep);
  int first_char = 0;
  for(int i = 0; i < len; i++) {
    if(str[i] == sep && !in_sep && i != len-1) {
      size_t new_len = i - first_char + 1;
      split_strings[str_idx] = malloc(new_len);
      memmove(split_strings[str_idx], &str[first_char], new_len-1);
      split_strings[str_idx][new_len-1] = '\0';
      str_idx++;
      in_sep = true;
    } else if (str[i] != sep) {
      if(in_sep) first_char = i;
      in_sep = false;
    }
  }
  if(!in_sep) {
    size_t new_len = (len) - first_char+1;
    split_strings[str_idx] = malloc(new_len);
    memmove(split_strings[str_idx], &str[first_char], new_len-1);
    split_strings[str_idx][new_len-1] = '\0';
  }
  assert(str_idx+1 == out_num_strings[0]);

  return split_strings;
}
typedef struct {
	int samples_per_second;
	size_t data_length;
	int16_t* amplitude_data;
} waveform_t;
int write_int_wav_file(FILE* fp, waveform_t data) {
	if(fp == NULL) {
		fprintf(stderr, "File Pointer is a NULL pointer!");
		return -1;
	}
	if(data.amplitude_data == NULL) {
		fprintf(stderr, "Amplitude data is a NULL pointer!");
		return -1;
	}


	fputc('R', fp);
	fputc('I', fp);
	fputc('F', fp);
	fputc('F', fp);

	size_t writing_size = 36 + data.data_length*2;
	fputc((writing_size&0xFF), fp);
	fputc((writing_size&0xFF00)>>8, fp);
	fputc((writing_size&0xFF0000)>>16, fp);
	fputc((writing_size&0xFF000000)>>24, fp);

	fputc('W', fp);
	fputc('A', fp);
	fputc('V', fp);
	fputc('E', fp);

	fputc('f', fp);
	fputc('m', fp);
	fputc('t', fp);
	fputc(' ', fp);

	fputc(16, fp);
	fputc(0, fp);
	fputc(0, fp);
	fputc(0, fp);
	/* fmt */
	fputc(1, fp);
	fputc(0, fp);
	/* channels */
	fputc(1, fp);
	fputc(0, fp);

	fputc((data.samples_per_second&0xFF), fp);
	fputc((data.samples_per_second&0xFF00)>>8, fp);
	fputc((data.samples_per_second&0xFF0000)>>16, fp);
	fputc((data.samples_per_second&0xFF000000)>>24, fp);

	fputc(((data.samples_per_second*2)&0xFF), fp);
	fputc(((data.samples_per_second*2)&0xFF00)>>8, fp);
	fputc(((data.samples_per_second*2)&0xFF0000)>>16, fp);
	fputc(((data.samples_per_second*2)&0xFF000000)>>24, fp);

	fputc(2, fp);
	fputc(0, fp);

	fputc(16, fp);
	fputc(0, fp);

	fputc('d', fp);
	fputc('a', fp);
	fputc('t', fp);
	fputc('a', fp);

	fputc(((data.data_length*2)&0xFF), fp);
	fputc(((data.data_length*2)&0xFF00)>>8, fp);
	fputc(((data.data_length*2)&0xFF0000)>>16, fp);
	fputc(((data.data_length*2)&0xFF000000)>>24, fp);

	//printf("len %lli\n", data.data_length);

	for(size_t i = 0; i < data.data_length; i++) {
		fputc((data.amplitude_data[i]&0xFF), fp);
		fputc((data.amplitude_data[i]&0xFF00)>>8, fp);
	}
	
	return 0;
}
int execute_cmd(char* path, int cmd_argc, char** cmd_argv, char** cmd_env, char* outpath, char* errpath, bool out_append, bool err_append) {
  // from SEI Cert example
  pid_t pid;
  int status;
  pid_t ret;
  char **env;

  // the +1 with calloc creates a NULL-pointer at the end of the array
  char **args = calloc(cmd_argc+1, sizeof(char*));
  for(int i = 0; i < cmd_argc; i++) {
    args[i] = cmd_argv[i];
  }


  pid = fork();
  if (pid == -1) {
    /* Handle error */
    fprintf(stderr, "Porgram could not fork to create process\n");
    exit(127);
  } else if (pid != 0) {
    while ((ret = waitpid(pid, &status, 0)) == -1) {
      if (errno != EINTR) {
        /* Handle error */
        // TODO
        break;
      }
    }
    if ((ret != -1) && (!WIFEXITED(status) || !WEXITSTATUS(status)) ) {
      /* Report unexpected child status */
      // TODO
    }
    return ret;
  } else {

    if(outpath != NULL) {
      if(out_append) {
        stdout = freopen(outpath, "a", stdout);
      } else {
        stdout = freopen(outpath, "w", stdout);
      }
    }



    if(errpath != NULL) {
      if(err_append) {
        stderr = freopen(errpath, "a", stderr);
      } else {
        stderr = freopen(errpath, "w", stderr);
      }
    }


    if (execve(path, args, cmd_env) == -1) {
      /* Handle error */
      _Exit(127);
    }
    
    __builtin_unreachable();
    return 0;
  }
}



size_t filesize(const char* path) {
    struct stat buf;
    
    if(path == NULL) return (size_t) -1;
    
    stat(path, &buf);
    off_t size = buf.st_size;
    return size;
}

bool read_praat_formant_output_file(const char* path, int* nr_formants, int* nr_frames, float*** data) {
    if(path == NULL || nr_formants == NULL || nr_frames == NULL || data == NULL) return false;
    
    size_t len = filesize(path);
    char* text = calloc(len, 1);
    FILE* fp = fopen(path, "rb");
    size_t nr_read = fread(text, 1, len, fp);
    fclose(fp);
    if(nr_read != len) return false;
    
    int nr_lines;
    char** lines = split(text, len, '\n', &nr_lines);
    free(text);
    nr_frames[0] = nr_lines-1;
    data[0] = calloc(sizeof(float*), nr_lines-1);
    //printf("nr_frames = %i\n", nr_lines-1);
    
    int nr_columns;
    for(int i = 0; i < nr_lines; i++) {
        int local_nr_columns;
        char** columns = split(lines[i], strlen(lines[i]), '\t', &local_nr_columns);
        
        //printf("%i: columns: %i \n", i, local_nr_columns);
        //for(int k = 0; k < local_nr_columns; k++) {
        //    printf("%i:%i : \"%s\" \n", i, k, columns[k]);
        //}
        
        if(i == 0) {
            nr_columns = local_nr_columns;
            nr_formants[0] = (local_nr_columns - 1)/2;
        } else {
            if (local_nr_columns != nr_columns) return false;
            data[0][i-1] = calloc(nr_formants[0], sizeof(float));
            for(int k = 0; k < nr_formants[0]; k++) {
                const char* column_text = columns[2*k+1];
                if(strcmp(column_text, "--undefined--") == 0) {
                    data[0][i-1][k] = -1.0;
                } else {
                    data[0][i-1][k] = (float) atof(columns[2*k+1]);
                }
            }
        }
        
        for(int i = 0; i < local_nr_columns; i++) {
            free(columns[i]);
        }
        free(columns);
        free(lines[i]);
    }
    free(lines);
    
    return true;
}


void read_formants_from_wave_file_via_praat(char* filename, float* F1, float* F2, float* F3, float* F4, char** envp) {
    F1[0] = F2[0] = F3[0] = F4[0] = 0.0;
    
    char* argv[3];
     argv[0] = "praat";
     argv[1] = "--run";
     argv[2] = "temp.praat";
     
     
     FILE* praat_script_write = fopen("temp.praat", "w");
     fprintf(praat_script_write,
"Erase all\n"
"Select outer viewport: 0, 6.5, 0, 4\n"
"Read from file: \"%s\"\n"
"To Formant (burg): 0, 5, 5500, 0.025, 50\n"
"#Draw tracks: 0, 0, 5500, yes\n"
"List: \"yes\", \"no\", 3, \"no\", 2, \"no\", 2, \"yes\"\n",
    filename);
    fclose(praat_script_write);
    
    
    (void) execute_cmd("/home/sva/bin/praat", 3, argv, envp, "output.txt", NULL, false, false);
    
    remove("temp.praat");
    
    
    char* folder = "/home/sva/projects/mic-feedback_app/";
    
    
    if(chdir(folder) != 0) {
        die("error! directory could not be changed.");
    }
    
    
    float** data; int nr_formants; int nr_frames;
    bool code = read_praat_formant_output_file("output.txt", &nr_formants, &nr_frames, &data);
    if(!code) { die("Error reading praat output file!"); }
            
            
            
    
            if(nr_formants >= 0) {
                float avg = 0.0;
                for(int i = 0; i < nr_frames; i++) {
                    avg += data[i][0];
                }
                F1[0] = avg/nr_frames;
                
            }
            if(nr_formants >= 1) {
                float avg = 0.0;
                for(int i = 0; i < nr_frames; i++) {
                    avg += data[i][1];
                }
                F2[0] = avg/nr_frames;
            }
            if(nr_formants >= 2) {
                float avg = 0.0;
                for(int i = 0; i < nr_frames; i++) {
                    avg += data[i][2];
                }
                F3[0] = avg/nr_frames;
            }
            if(nr_formants >= 3) {
                float avg = 0.0;
                for(int i = 0; i < nr_frames; i++) {
                    avg += data[i][3];
                }
                F4[0] = avg/nr_frames;
            }
            
            
            remove("output.txt");
            
}


int main(int argc, char** argv, char** envp) {
    const float c = 35300.0;
    float formants_larynx_check[5][4];
    float param[5];

    assert(argc >= 6);
    
    for(int i = 1; i <= 5; i++) {
        read_formants_from_wave_file_via_praat(argv[i], &formants_larynx_check[i][0],
            &formants_larynx_check[i][1], &formants_larynx_check[i][2], &formants_larynx_check[i][3], envp);
    }
    
    gsl_vector *y = gsl_vector_alloc(5);
    gsl_vector_set(y, 4, c / 17.0);
    gsl_vector_set(y, 3, c / 16.0);
    gsl_vector_set(y, 0, c / 15.0);
    gsl_vector_set(y, 1, c / 14.0);
    gsl_vector_set(y, 2, c / 13.0);

    gsl_matrix *X = gsl_matrix_alloc(5, 5);
    gsl_matrix_set(X, 0, 0, 1);
    gsl_matrix_set(X, 0, 1, formants_larynx_check[0][0]);
    gsl_matrix_set(X, 0, 2, formants_larynx_check[0][1]);
    gsl_matrix_set(X, 0, 3, formants_larynx_check[0][2]);
    gsl_matrix_set(X, 0, 4, formants_larynx_check[0][3]);
    gsl_matrix_set(X, 1, 0, 1);
    gsl_matrix_set(X, 1, 1, formants_larynx_check[1][0]);
    gsl_matrix_set(X, 1, 2, formants_larynx_check[1][1]);
    gsl_matrix_set(X, 1, 3, formants_larynx_check[1][2]);
    gsl_matrix_set(X, 1, 4, formants_larynx_check[1][3]);
    gsl_matrix_set(X, 2, 0, 1);
    gsl_matrix_set(X, 2, 1, formants_larynx_check[2][0]);
    gsl_matrix_set(X, 2, 2, formants_larynx_check[2][1]);
    gsl_matrix_set(X, 2, 3, formants_larynx_check[2][2]);
    gsl_matrix_set(X, 2, 4, formants_larynx_check[2][3]);
    gsl_matrix_set(X, 3, 0, 1);
    gsl_matrix_set(X, 3, 1, formants_larynx_check[3][0]);
    gsl_matrix_set(X, 3, 2, formants_larynx_check[3][1]);
    gsl_matrix_set(X, 3, 3, formants_larynx_check[3][2]);
    gsl_matrix_set(X, 3, 4, formants_larynx_check[3][3]);
    gsl_matrix_set(X, 4, 0, 1);
    gsl_matrix_set(X, 4, 1, formants_larynx_check[4][0]);
    gsl_matrix_set(X, 4, 2, formants_larynx_check[4][1]);
    gsl_matrix_set(X, 4, 3, formants_larynx_check[4][2]);
    gsl_matrix_set(X, 4, 4, formants_larynx_check[4][3]);
    
    
    gsl_permutation* p = gsl_permutation_alloc(5);
    int signum;
    if(gsl_linalg_LU_decomp(X, p, &signum) != GSL_SUCCESS)
        die("gsl_linalg_LU_decomp failed!");
    
    
    gsl_vector *x = gsl_vector_alloc(5);
    (void) gsl_linalg_LU_solve(X, p, y, x);
    

    printf("param = ");
    for(int i = 0; i < 5; i++) {
        printf("%f, ", gsl_vector_get(x, i));
    }
    printf("\n");
    param[0] = gsl_vector_get(x, 0);
    param[1] = gsl_vector_get(x, 1);
    param[2] = gsl_vector_get(x, 2);
    param[3] = gsl_vector_get(x, 3);
    param[4] = gsl_vector_get(x, 4);
    
    
    
    float F1 = 0.0, F2 = 0.0, F3 = 0.0, F4 = 0.0;
    
    read_formants_from_wave_file_via_praat(argv[6], &F1, &F2, &F3, &F4, envp);
    
    
    float polynom_result = param[0] + F1*param[1] + F2*param[2] + F3*param[3] + F4*param[4];
    float vtl = c / polynom_result;
    
    printf("\n estimated VTL = %f \n", vtl);
            
            
}


