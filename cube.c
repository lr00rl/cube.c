#include <math.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
void usleep(__int64 usec)
{
  HANDLE timer;
  LARGE_INTEGER ft;

  ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

  timer = CreateWaitableTimer(NULL, TRUE, NULL);
  SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
  WaitForSingleObject(timer, INFINITE);
  CloseHandle(timer);
}
#endif

float A, B, C;

float cubeWidth = 20;
int width = 160, height = 44;
// use zbuffer to store the distance of each pixel from the camera
// also used for hidden surface removal
float zBuffer[160 * 44];
char buffer[160 * 44];
int backgroundASCIICode = '.';
int distanceFromCam = 100;
float horizontalOffset;
float K1 = 10;

float incrementSpeed = 0.6;

float x, y, z;
float ooz;
int xp, yp;
int idx;

float calculateX(int i, int j, int k) {
  return j * sin(A) * sin(B) * cos(C) - k * cos(A) * sin(B) * cos(C) +
         j * cos(A) * sin(C) + k * sin(A) * sin(C) + i * cos(B) * cos(C);
}

float calculateY(int i, int j, int k) {
  return j * cos(A) * cos(C) + k * sin(A) * cos(C) -
         j * sin(A) * sin(B) * sin(C) + k * cos(A) * sin(B) * sin(C) -
         i * cos(B) * sin(C);
}

float calculateZ(int i, int j, int k) {
  return k * cos(A) * cos(B) - j * sin(A) * cos(B) + i * sin(B);
}

// calculate the one point on the surface of the cube
// `xp = (int)(width / 2 + horizontalOffset + K1 * ooz * x * 2)`
// `yp = (int)(height / 2 + K1 * ooz * y)`
// 这两行代码是在进行3D到2D的投影变换，也就是将3D空间中的点转换到2D屏幕坐标系统中。让我逐个解释各个部分：
// 1. `ooz` 是 "one over z" 的缩写，即 1/z。这是一个透视投影中常用的值：
//    - 随着物体离摄像机越远(z值越大)，ooz就越小
//    - 这会导致物体看起来更小，这正是我们在现实世界中看到的透视效果
// 2. `K1` 是一个缩放因子，用于控制投影的缩放程度。它可能是一个预设的常量，用于调整整体的投影效果。
// 3. 对于 xp 的计算：
//    - `width / 2` 将原点移到屏幕的水平中心
//    - `horizontalOffset` 允许额外的水平偏移调整
//    - `K1 * ooz * x * 2` 是实际的投影计算，物体越远（z越大，ooz越小）投影就越小
// 4. 对于 yp 的计算：
//    - `height / 2` 将原点移到屏幕的垂直中心
//    - `K1 * ooz * y` 同样是投影计算
// 这是一个简化的透视投影公式，用于创建基本的3D效果。真实的3D图形引擎通常使用更复杂的矩阵变换，但这个简化版本的原理是相同的：随着物体远离摄像机，它在屏幕上显示的尺寸会变小。
// 
// 让我详细解释一下为什么需要 K1 这个缩放因子：
// 1. `ooz` (1/z) 本身确实已经提供了基本的透视效果：
//    - 当物体离摄像机远时(z大)，ooz小，投影尺寸小
//    - 当物体离摄像机近时(z小)，ooz大，投影尺寸大
// 2. 但是仅用 ooz 可能会带来两个问题：
//    - 投影可能太大或太小：如果不加缩放因子，当物体非常近时(z很小)，投影可能会超出屏幕；当物体很远时(z很大)，投影可能小到几乎看不见
//    - 缺乏控制：没有办法调整整体的投影大小来适应不同的显示需求
// 3. 这就是为什么需要 K1：
//    ```c
//    xp = (int)(width / 2 + horizontalOffset + K1 * ooz * x * 2)
//    ```
//    - K1 让我们可以统一调整投影的大小
//    - 如果投影整体太大，我们可以减小 K1
//    - 如果投影整体太小，我们可以增大 K1
//    - 这样就不需要修改原始的 3D 坐标或摄像机位置
// 简单来说，K1 是一个"微调旋钮"，让我们可以在保持正确透视效果的同时，更好地控制最终画面的大小。这在实际应用中非常有用，因为我们经常需要根据不同的屏幕尺寸或显示需求来调整投影的大小。
//
// 在这两行代码中：
// ```c
// xp = (int)(width / 2 + horizontalOffset + K1 * ooz * x * 2)
// yp = (int)(height / 2 + K1 * ooz * y)
// ```
// x 坐标乘以 2 而 y 不乘，这通常是为了处理显示屏幕的宽高比（aspect ratio）问题：
// 1. 大多数显示设备的像素不是正方形的，或者说显示区域的宽高比不是 1:1
//    - 比如一个字符在终端中显示时，通常高度比宽度大
//    - 在字符终端中，一个字符的高度通常约为其宽度的两倍
// 2. 乘以 2 是在补偿这种宽高比的差异：
//    - 如果不乘以 2，由于字符的这种特性，投影出来的立方体会看起来被"压扁"了
//    - 乘以 2 后可以得到更接近正方形的投影效果
// 这是一种简单的宽高比矫正方法。在更专业的 3D 渲染系统中，这种矫正通常是通过投影矩阵来处理的，但在这个简化的实现中，直接乘以 2 是一个实用的解决方案。
void calculateForSurface(float cubeX, float cubeY, float cubeZ, int ch) {
  x = calculateX(cubeX, cubeY, cubeZ);
  y = calculateY(cubeX, cubeY, cubeZ);
  z = calculateZ(cubeX, cubeY, cubeZ) + distanceFromCam;

  ooz = 1 / z;

  xp = (int)(width / 2 + horizontalOffset + K1 * ooz * x * 2);
  yp = (int)(height / 2 + K1 * ooz * y);

  idx = xp + yp * width;
  if (idx >= 0 && idx < width * height) {
    if (ooz > zBuffer[idx]) {
      zBuffer[idx] = ooz;
      buffer[idx] = ch;
    }
  }
}

int main() {
  printf("\x1b[2J");
  while (1) {
    memset(buffer, backgroundASCIICode, width * height);
    memset(zBuffer, 0, width * height * 4);
    cubeWidth = 20;
    horizontalOffset = -2 * cubeWidth;
    // first cube
    for (float cubeX = -cubeWidth; cubeX < cubeWidth; cubeX += incrementSpeed) {
      for (float cubeY = -cubeWidth; cubeY < cubeWidth; cubeY += incrementSpeed) {
        // there are 6 surfaces in a cube
        // and for cubeX and cubeY from -cubeWidth to cubeWidth, it can be used to describe the each points in 6 surfaces
        // so, with the increment of cubeX and cubeY, we can describe all the points in 6 surfaces
        calculateForSurface(cubeX, cubeY, -cubeWidth, '@');
        calculateForSurface(cubeWidth, cubeY, cubeX, '$');
        calculateForSurface(-cubeWidth, cubeY, -cubeX, '~');
        calculateForSurface(-cubeX, cubeY, cubeWidth, '#');
        calculateForSurface(cubeX, -cubeWidth, -cubeY, ';');
        calculateForSurface(cubeX, cubeWidth, cubeY, '+');
      }
    }
    // cubeWidth = 10;
    // horizontalOffset = 1 * cubeWidth;
    // // second cube
    // for (float cubeX = -cubeWidth; cubeX < cubeWidth; cubeX += incrementSpeed) {
    //   for (float cubeY = -cubeWidth; cubeY < cubeWidth;
    //        cubeY += incrementSpeed) {
    //     calculateForSurface(cubeX, cubeY, -cubeWidth, '@');
    //     calculateForSurface(cubeWidth, cubeY, cubeX, '$');
    //     calculateForSurface(-cubeWidth, cubeY, -cubeX, '~');
    //     calculateForSurface(-cubeX, cubeY, cubeWidth, '#');
    //     calculateForSurface(cubeX, -cubeWidth, -cubeY, ';');
    //     calculateForSurface(cubeX, cubeWidth, cubeY, '+');
    //   }
    // }
    // cubeWidth = 5;
    // horizontalOffset = 8 * cubeWidth;
    // // third cube
    // for (float cubeX = -cubeWidth; cubeX < cubeWidth; cubeX += incrementSpeed) {
    //   for (float cubeY = -cubeWidth; cubeY < cubeWidth;
    //        cubeY += incrementSpeed) {
    //     calculateForSurface(cubeX, cubeY, -cubeWidth, '@');
    //     calculateForSurface(cubeWidth, cubeY, cubeX, '$');
    //     calculateForSurface(-cubeWidth, cubeY, -cubeX, '~');
    //     calculateForSurface(-cubeX, cubeY, cubeWidth, '#');
    //     calculateForSurface(cubeX, -cubeWidth, -cubeY, ';');
    //     calculateForSurface(cubeX, cubeWidth, cubeY, '+');
    //   }
    // }
    printf("\x1b[H");
    for (int k = 0; k < width * height; k++) {
      putchar(k % width ? buffer[k] : 10);
    }

    A += 0.05;
    B += 0.05;
    C += 0.01;
    usleep(8000 * 2);
  }
  return 0;
}
