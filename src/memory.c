#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <fcntl.h>
#include "subjectSetting.h"

// nanosleep() unit=SLEEP_UNI us
#define SLEEP_UNIT 100
#ifndef TIMEOUT
#define TIMEOUT 1
#endif

#define STD_EVALUATION_TIMES 1

GList* testcases;

gchar* CURRDIR;

gchar* TESTCASEDIR;

FILE* logfp;

double timeout_sec;

int allRequestPeak;

int noe;

GRand* randomUtility;

gint readProcMemory(gint i){
	gchar* filename=g_malloc(128*sizeof(gchar));
	gchar* line=g_malloc(128*sizeof(gchar));
	g_snprintf(filename, 128, "%sprocMemory%d", CURRDIR, i);
	if(!g_file_test(filename, G_FILE_TEST_EXISTS)){
		fprintf(stderr, "Warning, procMemory does not exist.\n");
		return 0;
	}
	FILE* fp=fopen(filename, "r");
	gint max=0, one=0, sum=0, requestSum=0, requestMax=0, j;
	while(!feof(fp)){
		if(!fgets(line, 128, fp)){
			break;
		}
		if(g_str_has_prefix(line, "memory:")){
			sscanf(line, "memory: %d\t%d\t%d", &j, &one, &requestSum);
			noe++;
			sum+=one;
			if(sum>max) max=sum;
			if(requestSum>requestMax) requestMax=requestSum;
		}
	}
	g_free(filename);
	g_free(line);
	fclose(fp);
	allRequestPeak+=requestMax;
	return max;
}

gint cmpOutput(gchar* filename1, gchar* filename2){
	FILE* fp1=fopen(filename1, "r");
	FILE* fp2=fopen(filename2, "r");
	if(!fp1 || !fp2){
		fprintf(logfp, "open camparison files fail.\n");
		return 1;
	}
	gint flag=1;
	gchar* line1=g_malloc0(256*sizeof(gchar));
	gchar* line2=g_malloc0(256*sizeof(gchar));
	while(!feof(fp1) && !feof(fp2)){
		flag=0;
		line1=fgets(line1, 256, fp1);
		line2=fgets(line2, 256, fp2);
		if(line1==NULL && line2==NULL) break;
		if(line1==NULL || line2==NULL){
			flag=1;
			break;
		}
		if(strcmp(line1, line2)){
			flag=1;
			//fprintf(logfp, "check point 3 in cmpOutput().\n");
			//fflush(logfp);
			break;
		}
	}
	if(line1) g_free(line1);
	if(line2) g_free(line2);
	fclose(fp1);
	fclose(fp2);
	return flag;
}

gint mycp(const char *from, const char *to)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

  out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}

gint profile(double* time_usr, double* time_sys, double* memory, double* correctness, gint suiteSize, gint isStd){
	double tusr=0, tsys=0, correctness_t=0;
	double stdTime=0;
	gint stdMemory=0;
	gint memory_new_one=0;
	double memory_new=0;
	allRequestPeak=0;
	noe=0;
	gchar* line;
	gchar* filename=g_malloc(128*sizeof(gchar));
	gchar* append=g_malloc(8*sizeof(gchar));
	if(isStd) g_snprintf(append, 32, ".s");
	else append[0]='\0';
	gint i, j, k, length=g_list_length(testcases);
	//gint fullTest=1;
	gint* testSuite=g_malloc0(length*sizeof(gint));
	double* testResults=g_malloc0(length*sizeof(double));
	gint* testMemory=g_malloc0(length*sizeof(gint));
	gint flippedSize, index;
	FILE* resultsFile;

	if(isStd>0 || suiteSize<=0 || suiteSize>length){
		suiteSize=length;
		for(i=0; i<suiteSize; i++){
			testSuite[i]=1;
		}
	}
	else{		// randomly sample testcases
		if(suiteSize>length/2){
			flippedSize=length-suiteSize;
		}
		else{
			flippedSize=suiteSize;
		}
		index=0;
		for(i=0; i<flippedSize; i++){
			j=g_rand_int_range(randomUtility, 1, length+1-i);
			while(j>0){
				index++;
				if(index>=length){
					index-=length;
				}
				if(testSuite[index]==0){
					j--;
				}
			}
			testSuite[index]=1;
		}
		if(flippedSize!=suiteSize){
			for(i=0; i<length; i++){
				testSuite[i]=1-testSuite[i];
			}
		}
	}
	// read testResults file
	if(!isStd){
		g_snprintf(filename, 128, "%stestResults.txt", CURRDIR);
		resultsFile=fopen(filename, "r");
		for(i=0; i<length; i++){
			fgets(filename, 128, resultsFile);
			sscanf(filename, "%lf\t%d", &testResults[i], &testMemory[i]);
			if(testSuite[i]){
				stdTime+=testResults[i];
				stdMemory+=testMemory[i];
			}
		}
		fclose(resultsFile);
	}
	// clean the output files
	for(i=0; i<length; i++){
		g_snprintf(filename, 128, "%sout%d%s", CURRDIR, i, append);
		if(g_file_test(filename, G_FILE_TEST_EXISTS)){
			g_remove(filename);
		}
	}
	line=g_malloc(512*sizeof(gchar));
	gchar **cmdv=NULL;
	gint pid, mpid;
	FILE *mfp;
	tusr=0;
	tsys=0;
	double tmp_double;
	gint tmp_int, timeout;
	struct rusage usage;
	struct timespec sleepUnit;
	sleepUnit.tv_sec=0;
	sleepUnit.tv_nsec=SLEEP_UNIT*1000;
	gint stdEvalTimes;
	if(isStd){
		stdEvalTimes=STD_EVALUATION_TIMES;
	}
	else{
		stdEvalTimes=1;
	}
	for(k=0; k<stdEvalTimes; k++){
		for(i=0; i<length; i++){
			if(testSuite[i]==0){
				continue;
			}
			j=i;
			g_snprintf(filename, 128, "%sout%d%s", CURRDIR, j, append);
			g_snprintf(line, 512, "subject %s%s", TESTCASEDIR, (char*)g_list_nth_data(testcases, j));
			g_shell_parse_argv(line, NULL, &cmdv, NULL);
			if((pid=fork())<0){
				g_printf("fork() for subject execution failed.\n");
				exit(0);
			}
			if(pid==0){
				if(g_freopen(filename, "w", stdout)==NULL) perror("redirect to file failed.");
				g_snprintf(filename, 128, "%sprocMemory%d", CURRDIR, j);
				if(g_freopen(filename, "w", stderr)==NULL) perror("redirect stderr to file failed.");
				execv("subject", cmdv);
				fprintf(logfp, "execv() failed.\n");
				fflush(logfp);
				exit(0);
			}
			g_strfreev(cmdv);

			// parent process, wait for pid
			if(isStd){
				wait4(pid, &tmp_int, 0, &usage);
			}
			else{
				timeout=(gint)(timeout_sec*1000000/SLEEP_UNIT);
				while(timeout>0 && wait4(pid, &tmp_int, WNOHANG, &usage)==0){
					timeout--;
					nanosleep(&sleepUnit, NULL);
				}
				if(timeout<=0){
					kill(pid, SIGKILL);
					waitpid(pid, &tmp_int, 0);
					break;
				}
			}
#ifdef CUSTOM_EXTRA_SAVING
			CUSTOM_EXTRA_SAVING;
#endif
			//g_spawn_close_pid(pid);
			memory_new_one=readProcMemory(j);
			if(isStd && k==0){
				testMemory[i]=memory_new_one;
			}
			memory_new+=memory_new_one;
			tmp_double=usage.ru_utime.tv_sec+(usage.ru_utime.tv_usec)/(double)1000000;
			if(isStd){
				testResults[i]+=tmp_double;
			}
			tusr+=tmp_double;
			tmp_double=usage.ru_stime.tv_sec+(usage.ru_stime.tv_usec)/(double)1000000;
			if(isStd){
				testResults[i]+=tmp_double;
			}
			tsys+=tmp_double;
			// when the total execution time exceeds time limit, directly return
			if(isStd==0){
				g_snprintf(filename, 128, "%sout%d", CURRDIR, j);
				g_snprintf(line, 128, "%sout%d.s", CURRDIR, j);
				if(cmpOutput(filename, line) || tusr+tsys>timeout_sec){
					timeout=-1;
					break;
				}
			}
		}
	}
	// cmp the output and summarise the correctness
	if(timeout<=0){
		correctness_t=i+1;
	}
	else{
		tusr/=stdEvalTimes;
		tsys/=stdEvalTimes;
		memory_new/=stdEvalTimes;
		if(isStd==0){
#ifdef CUSTOM_CORRECTNESS
			CUSTOM_CORRECTNESS;
#else
			for(i=0; i<length; i++){
				if(testSuite[i]==0){
					continue;
				}
				g_snprintf(filename, 128, "%sout%d", CURRDIR, i);
				g_snprintf(line, 128, "%sout%d.s", CURRDIR, i);
				correctness_t+=cmpOutput(filename, line);
			}
#endif
			tusr/=stdTime;
			tsys/=stdTime;
			memory_new/=stdMemory;
		}
		else{
			g_snprintf(filename, 128, "%stestResults.txt", CURRDIR);
			resultsFile=fopen(filename, "w+");
			for(i=0; i<length; i++){
				g_fprintf(resultsFile, "%lf\t%d\n", testResults[i]/stdEvalTimes, testMemory[i]);
			}
			fclose(resultsFile);
			memory_new/=1024;
		}
	}
	g_free(filename);
	g_free(append);
	g_snprintf(line, 128, "%lf %lf %lf %lf\n", tusr, tsys, memory_new, correctness_t);
	gint lineLength=strlen(line);
	write(STDOUT_FILENO, line, lineLength);
	//g_printf("%d\n", allRequestPeak/stdEvalTimes);
	g_free(line);
	g_free(testSuite);
	g_free(testResults);
	g_free(testMemory);
	*time_usr=tusr;
	*time_sys=tsys;
	*memory=memory_new/(double)1024;
	*correctness=correctness_t;
	return 0;
}

void readTestcases(){
	FILE* fp=fopen("testcases.txt", "r+");
	if(!fp){
		g_printf("Read Testcases fail.\n");
		return;
	}
	gchar* line;
	gint i;
	while(!feof(fp)){
		line=g_malloc0(128*sizeof(gchar));
		if(!fgets(line, 128, fp)){
			g_free(line);
			continue;
		}
		i=0;
		while(line[i]!='\n' && line[i]!='\0') i++;
		if(i==0) continue;
		if(line[i]=='\n') line[i]='\0';
		testcases=g_list_append(testcases, line);
	}
	fclose(fp);
}

void freeTestcases(){
	GList* p=testcases;
	gchar* s;
	while(p){
		s=(gchar*)(p->data);
		g_free(s);
		p->data=NULL;
		p=p->next;
	}
	g_list_free(testcases);
}

void main(int argc, char** argv){
	gint isStd=1;
	gint suiteSize=0;
	if(argc>=2){
		CURRDIR=argv[1];
		if(argc>=3){
			TESTCASEDIR=argv[2];
			if(argc>=4){
				isStd=atoi(argv[3]);
				if(argc>=5){
					timeout_sec=atof(argv[4]);
					if(argc>=6){
						suiteSize=atoi(argv[5]);
					
					}
				}
				else{
					timeout_sec=TIMEOUT;
				}
			}
			else{
				isStd=1;
				timeout_sec=TIMEOUT;
			}
		}
		else{
			TESTCASEDIR=DEFAULT_TESTCASES_DIR;
			isStd=1;
			timeout_sec=TIMEOUT;
		}
	}
	else{
		CURRDIR=DEFAULT_CURR_DIR;
		TESTCASEDIR=DEFAULT_TESTCASES_DIR;
		isStd=1;
		timeout_sec=TIMEOUT;
	}

	randomUtility=g_rand_new();
	logfp=fopen("memoryLog.txt", "w+");
	readTestcases();
fprintf(logfp, "timeout_sec=%lf\n", timeout_sec);
fflush(logfp);
	double time=0, time_usr=0, time_sys=0, memory=0, correctness=0;
	profile(&time_usr, &time_sys, &memory, &correctness, suiteSize, isStd);
	freeTestcases();
	fclose(logfp);
	g_rand_free(randomUtility);
}
