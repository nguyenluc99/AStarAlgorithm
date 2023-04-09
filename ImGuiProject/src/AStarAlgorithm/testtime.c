/* localtime example */
#include <stdio.h>
#include <time.h>
#include <unistd.h>
// #include <chrono>
// #include <thread>

// void msleep(int millisecs)
// {
//     std::this_thread::sleep_for(std::chrono::milliseconds(millisecs));
// }

int main ()
{
  time_t rawtime;
  struct tm * timeinfo;
  

  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  printf ( "Current local time and date: %s", asctime (timeinfo) );
  printf ( "Current local time and date: %ld\n", time(0) );
  usleep(1000);
  printf ( "Current local time and date: %ld\n", time(0));
  struct timeval now;
    gettimeofday(&now, NULL);
  printf ( "Current local time and date: %d - %ld\n", now.tv_usec, now.tv_sec);
  usleep(1000000);
    gettimeofday(&now, NULL);
  printf ( "Current local time and date: %d - %ld\n", now.tv_usec, now.tv_sec);
  
  return 0;
}