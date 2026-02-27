#pragma once
#include "queue.h"

#define SCALE_MEASSURES_NUM 1

extern float calibValue;
extern float scaleValue;
extern bool scaleCalibrate;
extern bool scaleTare;

extern float weightFromQueue;

extern QueueHandle_t weightQueue;
void scaleTask(void *pvParameters);
void calibrateScale(float calWeight);

void scale_begin(void);

bool scale_read(float *val);