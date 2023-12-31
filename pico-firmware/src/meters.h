#ifndef METERS_H_
#define METERS_H_

void meters_setup(void);
void meters_receiveSerialData(void);
void meters_updateStats(void);
void meters_updateMeters(void);
void meters_screenSaver(void);
void updateLastValueReceived(int idx, int val);
void updateLastTimeReceived(void);
long map(long x, long in_min, long in_max, long out_min, long out_max);

enum {
    CPU = 0,
    MEM,
    NUMBER_OF_METERS,
};

#endif // METERS_H_
