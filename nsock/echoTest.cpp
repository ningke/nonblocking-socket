#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <string>
#include <set>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "gtest/gtest.h"
#include "nsock.h"
#include "npoll.h"


using namespace std;
using namespace nsock;
using namespace npoll;


/*
 * Test the circular buffer.
 */
class CircularBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    CircularBuffer cBuffer;
};


/*
 * Write to circular buffer until it is full.
 */
TEST_F(CircularBufferTest, WriteUntilFull) {
    size_t bufsz = 64;
    uint8_t *buf = new uint8_t[bufsz];

    size_t freeSpc = cBuffer.dataBufSize;
    do {
        auto written = cBuffer.put(buf, bufsz);
        ASSERT_EQ(written, min(bufsz, cBuffer.dataBufSize));
        freeSpc -= written;
    } while (freeSpc > 0);

    delete [] buf;
}

/*
 * Read from the buffer until it is empty.
 */
TEST_F(CircularBufferTest, ReadUntilEmpty) {
    size_t bufsz = 256;
    uint8_t *buf = new uint8_t[bufsz];

    /* Write until full */
    size_t freeSpc = cBuffer.dataBufSize;
    do {
        auto written = cBuffer.put(buf, bufsz);
        ASSERT_EQ(written, min(bufsz, cBuffer.dataBufSize));
        freeSpc -= written;
    } while (freeSpc > 0);

    /* Read until empty */
    uint8_t *bufPtr = nullptr;
    size_t read = 0;
    do {
        read += cBuffer.get(&bufPtr);
    } while (bufPtr);

    ASSERT_EQ(read, cBuffer.dataBufSize);
    ASSERT_EQ(cBuffer.dataLen, 0UL);

    delete [] buf;
}

/*
 * Write some data, then read from the buffer and ensure data integrity is
 * preserved.
 */
TEST_F(CircularBufferTest, WriteAndReadWithWrapAround) {
    const size_t testWriteSize = (cBuffer.dataBufSize + 13331) * 13;
    size_t wbufSize = cBuffer.dataBufSize;
    uint8_t *wbuf = new uint8_t[wbufSize];

    size_t totalWritten = 0, totalRead = 0;
    uint8_t writeByte = 0, readByte = 0;
    srand(::time(NULL));

    while (totalWritten < testWriteSize) {
        // Write some data (value: 0-255 wraparound) of random length

        size_t wsize = rand() % wbufSize;
        for (size_t i = 0; i < wsize; i++) {
            wbuf[i] = writeByte++;
        }

        auto written = cBuffer.put(wbuf, wsize);
        totalWritten += written;
        ASSERT_LE(written, min(wsize, cBuffer.dataBufSize));
        if (written < wsize) {
            writeByte -= (wsize - written);
        }

        // Read some out
        uint8_t *bufPtr = nullptr;
        size_t read = cBuffer.get(&bufPtr);
        totalRead += read;
        ASSERT_LE(read, cBuffer.dataBufSize);
        ASSERT_LE(totalRead, totalWritten);

        for (size_t i = 0; i < read; i++) {
            ASSERT_EQ(bufPtr[i], readByte++);
        }
    }

    delete [] wbuf;
}


int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
