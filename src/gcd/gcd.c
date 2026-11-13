#include "gcd.h"

uint32_t gcd_u32(uint32_t a, uint32_t b)
{
    if (a == 0)
        return b;

    if (b == 0)
        return a;

    unsigned shift = 0;

    while (((a | b) & 1U) == 0) {
        a >>= 1;
        b >>= 1;
        ++shift;
    }

    while ((a & 1U) == 0)
        a >>= 1;

    do {
        /*本次更新Lright，一次檢查多個零，有連續零出現可以一次右移，不用跑回圈一次一次檢查*/
        if ((b & 1U) == 0) {
            if ((b & 3U) != 0) {
                b >>=
                    1; /*a1如果已經是奇數就不用位移,並檢查第0,1,2位是不是0，是的話就去後面檢查更多位的狀況，不是的話就一位一位移動*/
            } else {
                if ((b & 0xFFFFU) == 0)
                    b >>=
                        16; /*先檢查前16位是不是都是0，是的話一次移動完，不是的話往下檢查*/
                if ((b & 0xFFU) == 0)
                    b >>= 8;
                if ((b & 0xFU) == 0)
                    b >>= 4;
                if ((b & 3U) == 0)
                    b >>= 2;
                if ((b & 1U) == 0)
                    b >>= 1;
            }
        }
        /*本次更新Lright，一次檢查多個零，有連續零出現可以一次右移，不用跑回圈一次一次檢查*/

        if (a > b) {
            uint32_t temp = a;
            a = b;
            b = temp;
        }

        b -= a;
    } while (b != 0);

    return a << shift;
}
