gcc -o recorder record_main.c alad.c snd.c -DSND_NO_CHECKS -DNDEBUG -g -Wall -I. -Wno-unused-variable -Wno-unused-but-set-variable -Wno-attributes -lgsl -lgslcblas -lm
gcc -o larynx_analyze larynx_analyze_main.c -g -Wall -I. -Wno-unused-variable -Wno-unused-but-set-variable -Wno-attributes
gcc -o mic_rec mic_record_main.c alad.c snd.c -DSND_NO_CHECKS -DNDEBUG -g -Wall -I. -Wno-unused-variable -Wno-unused-but-set-variable -Wno-attributes

gcc -o lar_est larynx_estimate_main.c alad.c snd.c -DSND_NO_CHECKS -DNDEBUG -g -Wall -I. -Wno-unused-variable -Wno-unused-but-set-variable -Wno-attributes -lgsl -lgslcblas -lm
