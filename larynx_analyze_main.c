
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
#define TIME_PER_ROUND_SECONDS 1

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


int main(int argc, char** argv, char** envp) {
    assert(argc >= 2);
    const char* filename = argv[1];
    
    SET_BINARY_MODE(stdout);
    
    char* folder = "/home/sva/projects/mic-feedback_app/";
            
    
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

    char* praat_argv[3];
    praat_argv[0] = "praat";
    praat_argv[1] = "--run";
    praat_argv[2] = "temp.praat";
    (void) execute_cmd("/home/sva/bin/praat", 3, praat_argv, envp, "output.txt", NULL, false, false);
           
    remove("temp.praat");
           
            
    if(chdir(folder) != 0) {
        die("error! directory could not be changed.");
    }

    float** data; int nr_formants; int nr_frames;
    bool code = read_praat_formant_output_file("output.txt", &nr_formants, &nr_frames, &data);
    if(!code) { printf("Error reading file!"); return -1; }
            
    /*
    printf("nr_formants = %i, nr_frames = %i\n", nr_formants, nr_frames);
    for(int i = 0; i < nr_frames; i++) {
        for(int k = 0; k < nr_formants; k++) {
            printf("%f\t", data[i][k]);
        }
        printf("\n");
    }
    printf("\n");
    */
    
    if(nr_formants >= 3) {
        
        float* vtls = calloc(nr_frames, sizeof(float)); int nr_vtls_calculated = 0;
        float avg_vtl = 0.0f;
        for(int i = 0; i < nr_frames; i++) {
            float f1 = data[i][0];
            float f2 = data[i][1];
            float f3 = data[i][2];
            if(f1 > 0.0f && f2 > 0.0f && f3 > 0.0f) {
                vtls[nr_vtls_calculated] = 35300.0 / (4*(262.0 + 0.14*f1 + (0.16/3)*f2 + (0.25/5)*f3));
                avg_vtl += vtls[nr_vtls_calculated];
                nr_vtls_calculated++;
                //printf("%f\n", vtls[i]);
            }
        }
        avg_vtl /= nr_vtls_calculated;
        //printf("Average VTL after Hirsch/Myers/Awan (2026): %f cm\t\t", avg_vtl);
        printf("%f, ", avg_vtl);
    }
    
    if(nr_formants >= 4) {
        
        float* vtls = calloc(nr_frames, sizeof(float)); int nr_vtls_calculated = 0;
        float avg_vtl = 0.0f;
        for(int i = 0; i < nr_frames; i++) {
            float f1 = data[i][0];
            float f2 = data[i][1];
            float f3 = data[i][2];
            float f4 = data[i][3];
            if(f1 > 0.0f && f2 > 0.0f && f3 > 0.0f) {
                vtls[nr_vtls_calculated] = 35300.0 / (4*(229.0 + 0.030*f1 + (0.082/3)*f2 + (0.124/5)*f3 + (0.354/7)*f4));
                avg_vtl += vtls[nr_vtls_calculated];
                nr_vtls_calculated++;
                //printf("%f\n", vtls[i]);
            }
        }
        avg_vtl /= nr_vtls_calculated;
        //printf("Average VTL after Myers/Mathy/Roy (2022): %f cm\t\t", avg_vtl);
        printf("%f, ", avg_vtl);
    }
    
    
    float* dfs = calloc(nr_frames*nr_formants, sizeof(float)); int nr_dfs_calculated = 0;
    float avg_df = 0.0f;
    for(int i = 0; i < nr_frames; i++) {
        for(int k = 1; k < nr_formants; k++) {
            if(data[i][k] > 0.0f && data[i][k-1] > 0.0f) {
                dfs[nr_dfs_calculated] = data[i][k] - data[i][k-1];
                avg_df += dfs[nr_dfs_calculated];
                nr_dfs_calculated++;
            }
        }
    }
    avg_df /= nr_dfs_calculated;
    //printf("Average VTL by dF after Anikin/Barreda/Reby (2023): %f cm\n", 35300.0 / (2.0*avg_df));
    printf("%f,", 35300.0 / (2.0*avg_df));
    
    
    if(nr_formants >= 0) {
        float avg = 0.0;
        for(int i = 0; i < nr_frames; i++) {
            if(data[i][0] > 0) {
                avg += data[i][0];
            }
        }
        printf("%f, ", avg/nr_frames);
    }
    if(nr_formants >= 1) {
        float avg = 0.0;
        for(int i = 0; i < nr_frames; i++) {
            if(data[i][1] > 0) {
                avg += data[i][1];
            }
        }
        printf("%f, ", avg/nr_frames);
    }
    if(nr_formants >= 2) {
        float avg = 0.0;
        for(int i = 0; i < nr_frames; i++) {
            if(data[i][2] > 0) {
                avg += data[i][2];
            }
        }
        printf("%f, ", avg/nr_frames);
    }
    if(nr_formants >= 3) {
        float avg = 0.0;
        for(int i = 0; i < nr_frames; i++) {
            if(data[i][3] > 0) {
                avg += data[i][3];
            }
        }
        printf("%f, ", avg/nr_frames);
    }
    if(nr_formants >= 4) {
        float avg = 0.0;
        for(int i = 0; i < nr_frames; i++) {
            if(data[i][4] > 0) {
                avg += data[i][4];
            }
        }
        printf("%f, ", avg/nr_frames);
    }
    printf("\n");
    
    remove("output.txt");

    return 0;
}
