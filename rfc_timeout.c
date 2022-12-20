#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>

#define STOUS 1000000

double estimated_rtt = 0.2; //TODO tempo em segundos estimado de rtt (obtido previamente?)
double sample_rtt = 0.2; //TODO tempo médio em segundos de rtt calculado no código
double deviation = 0; //TODO desvio

double sum_rtt = 0; //TODO soma de rtts
double num_samples = 0; //quantidade de amostras

struct timeval get_time_in_timeval(double time) 
{
    struct timeval tp;
    bzero(&tp,sizeof(struct timeval));
    double seconds = floor(time);
    tp.tv_sec = (time_t) seconds;
    tp.tv_usec = STOUS * (time - seconds);

    return tp;
}

double get_time_in_seconds(struct timeval *tp)
{
	gettimeofday(tp, 0);
	return ((double) tp->tv_sec + ((double) tp->tv_usec / STOUS) );
}

double get_timeout_in_ms()
{
    double alfa = 1 / 8;
    estimated_rtt = ((1 - alfa) * estimated_rtt) + (alfa * sample_rtt);

    double beta = 1 / 4;
    deviation = ((1 - beta) * deviation) + (beta * abs(sample_rtt - estimated_rtt));

    return(estimated_rtt + (4 * deviation));
}

void new_sample_rtt(double new_sample)  //Adiciona mais uma amostra de rtt a soma de rtt e recalcula rtt amostrado
{
    num_samples++;
    sum_rtt += new_sample;
    sample_rtt = sum_rtt / num_samples;
}