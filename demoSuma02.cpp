/*
 * This program show the concurrency/parallelization process to make a sumatory
 *
 * The core program realize a sumatory from zero to N; The summatory is realizated
 * in chunks, the core promgam create child process to make a partial sumatories in
 * the range assigned, then the child send the partial results to the parent process.
 * The father admin and collect the partial results to get the total result.
 *
 * The main program repeat the summatory using K childs in each time
 * and plot the time consumed in the task.
 *
 */


#include <iostream>
#include <cstring>
#include <numeric>
#include <ctime>
#include <ratio>
#include <chrono>

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>


/**
 * @brief plotGraph  utilitary function to display the time results
 *                   only need the y values, the x values are implicit in the data order
 * @param vals       the y-values to plot
 * @param YRange     the range
 * @param xScale     scale for the x-axis
 * @return           cv::Mat contents the plot image
 */
cv::Mat plotGraph(double *vals, int sizeVals, int YRange[2], int xScale )
{
    int itMin = vals[0], itMax = vals[0];

    for(int ii=1; ii<sizeVals; ++ii)
    {
        if( vals[ii] < itMin )
            itMin = vals[ii];

        if( itMax < vals[ii] )
            itMax = vals[ii];
    }

    double scale = 1.0/ceil(itMax- itMin);
    double bias = itMin;
    int rows = (YRange[1] - YRange[0]) + 40;
    int cols = sizeVals*xScale+40;

    cv::Mat image = cv::Mat::zeros( rows, cols, CV_8UC3 );

    image.setTo(0);

    // draw axis
    cv::line(image, cv::Point(20, rows - 20 ), cv::Point( 20, 20), cv::Scalar(166, 166, 166), 2);
    cv::line(image, cv::Point(20, rows - 20 ), cv::Point( cols-20, rows-20), cv::Scalar(166, 166, 166), 2);

    for (int i = 0; i < sizeVals; i++)
    {
        if( i<(int)sizeVals-1)
            cv::line(image, cv::Point(20 + i*xScale, rows - 20 - (vals[i] - bias)*scale*YRange[1]), cv::Point( 20 + (i+1)*xScale, rows - 20 - (vals[i+1] - bias)*scale*YRange[1]), cv::Scalar(255, 0, 0), 1);

        cv::circle(image,  cv::Point(20 + i*xScale, rows - 20 - (vals[i] - bias)*scale*YRange[1]), 5, cv::Scalar(0,255,120), CV_FILLED, 8, 0   );
    }

    return image;
}


/**
 * @brief sumSeries  perform the the sumatory
 * @param from       the from nuber to add
 * @param to         until to number to add, whitout included
 * @return           the result sumatory
 */
long sumSeries(long from, long to)
{
    // oh yes yes yes, we can use the gauss formula; but we can try
    // concurency and parallelization, so we need consume time to
    // ilustrate this, so ...

    long sum = 0;

    for(long ii=from; ii<to; ++ii)
        sum += ii;

    return sum;
}


/**
 * @brief do_summatory the admin process to perfom the summatory
 *                     from Zero to limitSup, the summatory is made in
 *                     chunks each one assigned to child process.
 * @param numChilds    numChilds to create for make the task
 * @param limitSup     the limit number to add in the summatory
 * @return             the summatory value computed
 */
unsigned long do_summatory( int numChilds, long limitSup  )
{
    int K = numChilds;
    unsigned long resultSummatory;

    if( K == 1 )  // Serial processing
    {
        return sumSeries(1,limitSup+1);
    }

    /// vars for admin childs
    pid_t pid;
    pid_t rc_pid;
    pid_t child_pids[K];

    /// vars for admin interprocess communication
    int  fd[2*K], nbytes;
    char message[1024];
    char readbuffer[80];

    /// vars for the task to do
    int deltaSum = limitSup/K;
    unsigned long partialSum = 0;
    unsigned long from, to;

    /// Creating child and assign work
    for (int ii=0; ii<K; ii++)
    {
            /// creating the communication channel
            pipe(fd+ii*2);

            /// try to create the child process
            switch(pid = child_pids[ii] = fork())
            {
                    case -1:
                            /// something went wrong
                            /// parent exits
                            printf("CRITICAL ERROR: went try create child: %d\n",ii);
                            perror("fork");
                            exit(1);

                    case 0:
                            /// Code for the children process
                            printf("Children %d-%d: start working now ... \n",ii, getpid() );
                            // Child process closes up input side of pipe
                            close(fd[0+ii*2]);

                            /// assign the work to do
                            from =  ii*deltaSum;
                            to  = (ii == K-1) ? limitSup + 1 : (ii+1)*deltaSum;
                            partialSum  = sumSeries( from, to );

                            /// send the result by the comunication channel
                            /// through the output side of pipe
                            sprintf(message, "%ld", partialSum);
                            int lengMessage = strlen( message )+1;
                            write(fd[1+ii*2], message, lengMessage);

                            /// terminate the child process and send
                            /// a signal to the father
                            exit(lengMessage);

                            break;
                            /*Missing code for parent process*/
            }
    }

    /*Parent process code*/
    if (pid!=0 && pid!=-1)
    {
        /// Collect the partial results
        resultSummatory = 0 ;
        for (int ii=0; ii<K; ii++)
        {
                int status;

                ///* Parent process closes up output side of pipe
                close(fd[1+ii*2]);

                rc_pid = waitpid(child_pids[ii], &status, 0);
                printf("\tPARENT: Child %d-%d finished work\n",ii,child_pids[ii]);

                /// Read the result in a string from the pipe, channel communication
                nbytes = read(fd[0+ii*2], readbuffer, /*WEXITSTATUS(status)*/ sizeof( readbuffer) );
                resultSummatory +=  std::atol(readbuffer);
        }

        /// return the final result
        return resultSummatory;
    }

    return 1; /// something went wrong
}



int main(void)
{
    long limitSup = 1E8;
    int maxNumberProcess = 20;

    /// vars for collect the time results and plot
    unsigned long valueComputed;

    double timeProcess[maxNumberProcess];

    for(int kk=1; kk < maxNumberProcess; ++kk)
    {
        printf("----------------------------------------\n");
        printf("\ttry using %d process: \n",kk);
        printf("----------------------------------------\n");

        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

        valueComputed = do_summatory(kk, limitSup);

        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);

        printf("----------------------------------------\n");
        printf("\tresult: %ld  It took me: %f  seconds\n\n",valueComputed, time_span.count());

        timeProcess[ kk ] =  time_span.count()*1000;
    }

    /// plot the time results
    int YRange[2] = {0, 640 };

    cv::Mat graph = plotGraph(  timeProcess,  maxNumberProcess,  YRange, 50);
    cv::imshow("#Process vs Time(ms)",graph);

    cv::waitKey();
}


