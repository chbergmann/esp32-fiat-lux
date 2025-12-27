/* sntp.h 
   Get the current time from the internet 
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void sntp_start(void);
void sntp_end(void);
void sntp_wait4time(void);

#ifdef __cplusplus
}
#endif