#ifndef ECHO_H
#define ECHO_H

void  Echo_Init(void);
void  Echo_SetAmount(float amount);   /* 0 = off ... 1 = loudest, longest echo */
float Echo_Process(float in);         /* one sample in, one sample out */

#endif /* ECHO_H */
