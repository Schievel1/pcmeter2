#ifndef METERS_H_
#define METERS_H_


void meters_setup(void);
void meters_receiveSerialData(void);
void meters_updateStats(void);
void meters_updateMeters(void);
void meters_screenSaver(void);
void updateLastValueReceived(int idx, int val);
void updateLastTimeReceived(void);

#endif // METERS_H_
