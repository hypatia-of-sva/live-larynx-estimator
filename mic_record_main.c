
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


int main(int argc, char** argv, char** envp) {
    snd_device_list_t dev_list; snd_result_t r;
    snd_recording_device_t rec_dev;
    SET_BINARY_MODE(stdout);
    
    r = snd_init(&dev_list);
    if(r != SND_OK) { printf("Error %i!", r); return r; }

    if(argc < 4) {
        printf("Available mics: %i\n", dev_list.nr_recording_devices);
        for(int i = 0; i < dev_list.nr_recording_devices; i++) {
            printf("%i: %s", i, dev_list.recording_devices[i]);
            if(i == dev_list.recording_devices_default_id)
                printf(" (default)");
            printf("\n");
        }
    } else {
        int dev_nr = atoi(argv[1]);
        assert(dev_nr >= 0 && dev_nr < dev_list.nr_recording_devices);
        const char* filename = argv[2];
        int TIME_PER_ROUND_SECONDS = atoi(argv[3]);

        size_t buffersize = 44100*TIME_PER_ROUND_SECONDS;

        r = snd_recording_device_open(dev_nr, SND_FORMAT_PCM_INT16_MONO, 44100, buffersize, &rec_dev);
        if(r != SND_OK) { printf("Error %i!", r); return r; }
        
        r = snd_recording_start(rec_dev);
        if(r != SND_OK) { printf("Error %i!", r); return r; }

        
        int16_t* buf = calloc(sizeof(int16_t), buffersize*10);
        assert(usleep(5*TIME_PER_ROUND_SECONDS*1000000) == 0);

        for(int i = 0;;i++) {
            size_t currently_recorded_samples = 0;
            while(currently_recorded_samples < buffersize) {
                r = snd_recording_get_nr_samples(rec_dev, &currently_recorded_samples);
                if(r != SND_OK) { printf("Error %i!", r); return r; }
            }
            
            
            r = snd_recording_retrieve_samples_nonblocking(rec_dev, buf, buffersize);
            if(r != SND_OK) { printf("Error %i!", r); return r; }

            bool is_all_0 = true;
            for(int i = 0; i < buffersize; i++) {
                if(buf[i] != 0) {
                    is_all_0 = false;
                    break;
                }
            }
            if(!is_all_0) break;
        }
        
        FILE* fp = fopen(filename, "wb");
        if(fp == NULL) die("error! wave file could not be created");
        
        waveform_t form;
        form.samples_per_second = 44100;
        form.data_length = buffersize;
        form.amplitude_data = buf;
        write_int_wav_file(fp, form);

        fclose(fp);

        r = snd_recording_stop(rec_dev);
        if(r != SND_OK) { printf("Error %i!", r); return r; }
        r = snd_recording_device_close(rec_dev);
        if(r != SND_OK) { printf("Error %i!", r); return r; }

    }

    r = snd_exit();
    if(r != SND_OK) { printf("Error %i!", r); return r; }
    
    return 0;
}
