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

#include <sys/wait.h>
#include <unistd.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>


/**
 * @brief The ResultTask struct
 * container results, time and computed value, for a task
 */
struct ResultTask
{
    double time;
    long value;
};


/**
 * @brief plotGraph  utilitary function to display the time results
 *                   only need the y values, the x values are implicit in the data order
 * @param vals       the y-values to plot
 * @param YRange     the range
 * @param xScale     scale for the x-axis
 * @return           cv::Mat contents the plot image
 */
cv::Mat plotGraph(std::vector<double>& vals, int YRange[2], int xScale = 10)
{
    auto it = minmax_element(vals.begin(), vals.end());
    double scale = 1./ceil(*it.second - *it.first);
    double bias = *it.first;
    int rows = (YRange[1] - YRange[0]) + 40;
    int cols = vals.size()*xScale+40;

    cv::Mat image = cv::Mat::zeros( rows, cols, CV_8UC3 );

    image.setTo(0);

    // draw axis
    cv::line(image, cv::Point(20, rows - 20 ), cv::Point( 20, 20), cv::Scalar(166, 166, 166), 2);
    cv::line(image, cv::Point(20, rows - 20 ), cv::Point( cols-20, rows-20), cv::Scalar(166, 166, 166), 2);

    for (int i = 0; i < (int)vals.size(); i++)
    {
        if( i<(int)vals.size()-1)
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
    int     fd[2*K], nbytes;
    std::string message;
    char    readbuffer[80];

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
                            std::cerr<<"CRITICAL ERROR: went try create child: "<<ii<<std::endl;
                            perror("fork");
                            exit(1);

                    case 0:
                            /// Code for the children process
                            std::cout<<"Children "<<ii<<"-"<<getpid()<<" : start working now ... "<<std::endl;
                            // Child process closes up input side of pipe
                            close(fd[0+ii*2]);

                            /// assign the work to do
                            from =  ii*deltaSum;
                            to  = (ii == K-1) ? limitSup + 1 : (ii+1)*deltaSum;
                            partialSum  = sumSeries( from, to );

                            /// send the result by the comunication channel
                            /// through the output side of pipe
                            message = std::to_string(partialSum);
                            write(fd[1+ii*2], message.c_str(), message.length()+1);

                            /// terminate the child process and send
                            /// a signal to the father
                            exit(message.length()+1);

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
                std::cout<<"\tPARENT: Child "<<ii<<"-"<<child_pids[ii]<<" finished work"<<std::endl;

                /// Read the result in a string from the pipe, channel communication
                nbytes = read(fd[0+ii*2], readbuffer, /*WEXITSTATUS(status)*/ sizeof( readbuffer) );
                //std::cout<<" \tReceived string: "<< readbuffer<<std::endl;
                resultSummatory +=  std::atol(readbuffer);
        }

        /// return the final result
        return resultSummatory;
    }

    return 1; /// something went wrong
}



int main(void)
{
    long limitSup = 1E6;
    int maxNumberProcess = 10;

    /// vars for collect the time results and plot
    unsigned long valueComputed;

    std::vector<double> timeProcess;

    for(int kk=1; kk < maxNumberProcess; ++kk)
    {
        std::cout<<"----------------------------------------"<<std::endl;
        std::cout<<"\ttry using "<<kk<<" process: "<<std::endl;
        std::cout<<"----------------------------------------"<<std::endl;

        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

        valueComputed = do_summatory(kk, limitSup);

        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);

        std::cout<<"----------------------------------------"<<std::endl;
        std::cout<<"\tresult:"<< valueComputed <<" It took me: " << time_span.count() << " seconds\n\n"<<std::endl;

        timeProcess.push_back( time_span.count()*1000);
    }

    /// plot the time results
    double maxValue = *std::max_element( timeProcess.begin(), timeProcess.end() );
    int YRange[2] = {0, 640 };

    cv::Mat graph = plotGraph(  timeProcess,  YRange, 50);
    cv::imshow("#Process vs Time(ms)",graph);

    cv::waitKey();
}


