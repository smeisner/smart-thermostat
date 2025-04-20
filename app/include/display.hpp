#pragma once

void display_lock();
void display_unlock();
void displayInit();

void setBrightness(uint32_t level);
uint32_t getBrightness();
void tftCreateTask();

void getRawPoint(int32_t *x, int32_t *y);

