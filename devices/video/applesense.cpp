/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/** @file DisplayID class implementation. */

#include <devices/video/applesense.h>
#include <loguru.hpp>

#include <cinttypes>
#include <map>
#include <string>
#include <array>
#include <cstring>
#include <algorithm>

/** Mapping between monitor IDs and their sense codes. */
const std::map<std::string, MonitorInfo> MonitorIdToCode = {
    { "MacColor21in", {
        0, 0x00,
        "kESCZero21Inch",
        "21\" RGB",
        "RGB 21\", 21\" Color, Apple 21S Color", {
            {1152, 870, 100     , 68.7  , 75   }
        }
    }},
    { "PortraitGS", {
        1, 0x14,
        "kESCOnePortraitMono",
        "Portrait Monochrome",
        "B&W 15\", Apple Portrait", {
            { 640, 870,  57.2832, 68.9  , 75   }
        }
    }},
    { "MacRGB12in", {
        2, 0x21,
        "kESCTwo12Inch",
        "12\" RGB",
        "12\" Apple RGB", {
            { 512, 384,  15.6672, 24.48 , 60.15}
        }
    }},
    { "Radius21in", {
        3, 0x31,
        "kESCThree21InchRadius",
        "21\" RGB (Radius)",
        "", {
            {1152, 870, 100     , 68.7  , 75   }
        }
    }},
    { "Radius21inGS", {
        3, 0x34,
        "kESCThree21InchMonoRadius",
        "21\" Monochrome (Radius)",
        "", {
            {1152, 870, 100     , 68.7  , 75   }
        }
    }},
    { "TwoPageGS", {
        3, 0x35,
        "kESCThree21InchMono",
        "21\" Monochrome",
        "B&W 21\", Apple 2 Page Mono", {
            {1152, 870, 100     , 68.7  , 75   }
        }
    }},
    { "NTSC", {
        4, 0x0A,
        "kESCFourNTSC",
        "NTSC",
        "", {
            { 512, 384,  12.2727, 15.7  , 59.94},
            { 640, 480,  12.2727, 15.7  , 59.94}
        }
    }},
    { "MacRGB15in", {
        5, 0x1E,
        "kESCFivePortrait",
        "Portrait RGB",
        "RGB 15\", 15\" Tilt", {
            { 640, 870,  57.2834,  0    , 75   }
        }
    }},
    { "Multiscan15in", {
        6, 0x03,
        "kESCSixMSB1",
        "MultiScan Band-1 (12\" thru 16\")",
        "Multiple Scan 13, 14\"", {
            { 640, 480,   0     ,  0    , 67   },
            { 832, 624,   0     ,  0    , 75   }
        }
    }},
    { "Multiscan17in", {
        6, 0x0B,
        "kESCSixMSB2",
        "MultiScan Band-2 (13\" thru 19\")",
        "Multiple Scan 16, 17\"", {
            { 640, 480,   0     ,  0    , 67   },
            { 832, 624,   0     ,  0    , 75   },
            {1024, 768,   0     ,  0    , 75   }
        }
    }},
    { "Multiscan20in", {
        6, 0x23,
        "kESCSixMSB3",
        "MultiScan Band-3 (13\" thru 21\")",
        "Multiple Scan 20, 21\"", {
            { 640, 480,   0     ,  0    , 67   },
            { 640, 480,   0     ,  0    ,120   }, // control; not platinum
            { 832, 624,   0     ,  0    , 75   },
            {1024, 768,   0     ,  0    , 74.9 },
            {1152, 870,   0     ,  0    , 75   },
            {1280, 960,   0     ,  0    , 75   },
            {1280,1024,   0     ,  0    , 75   }
        }
    }},
    { "HiRes12-14in", {
        6, 0x2B,
        "kESCSixStandard",
        "13\"/14\" RGB or 12\" Monochrome",
        "B&W 12\", 12\" Apple Monochrome, 13\" Apple RGB, Hi-Res 12-14\"", {
            { 640, 480,  30.24  , 35.0  , 66.7 },
        }
    }},
    { "PALEncoder", {
        7, 0x00,
        "kESCSevenPAL",
        "PAL",
        "PAL, NTSC/PAL (Option 1)", {
            { 640, 480,  14.75  , 15.625, 50   },
            { 768, 576,  14.75  , 15.625, 50   }
        }
    }},
    { "NTSCEncoder", {
        7, 0x14,
        "kESCSevenNTSC",
        "NTSC",
        "NTSC w/convolution (Alternate)", {
            { 512, 384,  12.2727,  0    ,  60  },
            { 640, 480,  12.2727,  0    ,  60  }
        }
    }},
    { "VGA-SVGA", {
        7, 0x17,
        "kESCSevenVGA",
        "VGA",
        "VGA", {
            { 640, 480,  0      , 0     , 60   },
            { 640, 480,  0      , 0     ,120   }, // control; not platinum
            { 800, 600,  0      , 0     , 60   },
            { 800, 600,  0      , 0     , 72   },
            { 800, 600,  0      , 0     , 75   },
            {1024, 768,  0      , 0     , 60   },
            {1024, 768,  0      , 0     , 70   },
            {1024, 768,  0      , 0     , 75   },
            {1280, 960,  0      , 0     , 75   },
            {1280,1024,  0      , 0     , 75   }
        }
    }},
    { "MacRGB16in", {
        7, 0x2D,
        "kESCSeven16Inch",
        "16\" RGB (GoldFish)",
        "RGB 16\", 16\" Color", {
            { 832, 624,  57.2832, 49.7  , 75   }
        }
    }},
    { "PAL", {
        7, 0x30,
        "kESCSevenPALAlternate",
        "PAL (Alternate)",
        "PAL w/convolution (Alternate) (Option 2)", {
            { 640, 480,  14.75  , 15.625, 50   },
            { 768, 576,  14.75  , 15.625, 50   }
        }
    }},
    { "MacRGB19in", {
        7, 0x3A,
        "kESCSeven19Inch",
        "Third-Party 19",
        "RGB 19\", 19\" Color", {
            {1024, 768,  80      , 0    , 74.9 }
        }
    }},
    { "DDC", {
        7, 0x3E,
        "kESCSevenDDC",
        "DDC display",
        "EDID", {
            {1024, 768,  80      , 0    ,  0   }
        }
    }},
    { "NotConnected", {
        7, 0x3F,
        "kESCSevenNoDisplay",
        "No display connected",
        "no-connect"
    }},
};

const std::map<std::string, std::string> MonitorAliasToId = {
    { "AppleVision1710", "HiRes12-14in" }
};

/*
CircuitDiagram
9 bits: 210 21 20 10
210: sense 2, sense 1, sense 0 relative to monitor ground: 0 = same (ground), 1 = different (high)
21 20 10: sense 2 - 1, sense 2 - 0, sense 1 - 0: 0 = no wire, 1 = ->|- diode, 2 = -|<- diode, 3 = straight wire
*/
typedef uint16_t CircuitDiagram;

typedef enum {
    connection2      = 8,
    connection1      = 7,
    connection0      = 6,

    connection21     = 4,
    connection20     = 2,
    connection10     = 0,
    connectionMask   = 3,
} CircuitDiagramBits;

typedef enum {
    connectionGround        = 0, // grounded
    connectionNoGround      = 1, // floating

    connectionOpen          = 0, // no connection
    connectionBackward      = 1, // destination will ground source    source -->|-- destination
    connectionForward       = 2, // source will ground destination    source --|<-- destination
    connectionBidirectional = 3, // either end will ground the other
} SenseConnection;

/*
CircuitOutput
9 bits: 210 10 20 21
210: sense 2, sense 1, sense 0 relative to ground: 0 = same (ground), 1 = different (high)
10: sense 1 and sense 0 relative to sense 2 when sense 2 is pulled low: 0 = same (ground), 1 = different (high)
20: sense 2 and sense 0 relative to sense 1 …
21: sense 2 and sense 1 relative to sense 0 …
*/
typedef uint16_t CircuitOutput;

typedef enum {
    sense2           = 8,
    sense1           = 7,
    sense0           = 6,

    sense2low_sense1 = 5, // sense 1 when sense 2 low
    sense2low_sense0 = 4, // sense 0 when sense 2 low
    sense1low_sense2 = 3, // sense 2 when sense 1 low
    sense1low_sense0 = 2, // sense 0 when sense 1 low
    sense0low_sense2 = 1, // sense 2 when sense 0 low
    sense0low_sense1 = 0, // sense 1 when sense 0 low
} CircuitOutputBits;

typedef enum {
    levelLow   = 0,
    levelHigh  = 1,
} SenseLevel;

#define BTST(x,b) (((x) >> (b))&1)
#define BSR(x,b) ((x) >> (b))
#define BAND(x,b) ((x) & (b))

std::string MonitorCodeToDescription(CircuitOutput circuitOutput) {
    std::string description;
   for (auto it = MonitorIdToCode.begin(); it != MonitorIdToCode.end(); it++) {
        if (circuitOutput == ((it->second.std_sense_code << 6) | it->second.ext_sense_code))
            description += std::string(" ") + it->second.name;
    }
    if (
        (!BTST(circuitOutput, sense2) && ((circuitOutput | 0b011110101) != 0b011110101)) ||
        (!BTST(circuitOutput, sense1) && ((circuitOutput | 0b101011110) != 0b101011110)) ||
        (!BTST(circuitOutput, sense0) && ((circuitOutput | 0b110101011) != 0b110101011))
    ) {
        // This should never happen.
        description += " (invalid)";
    }

    {
        const int ddc1_ddc2b = (1 << sense2) | (1 << sense1) | (1 << sense2low_sense1) | (1 << sense1low_sense2);
        const int ddc1       = (1 << sense2);

        if ((circuitOutput & ddc1_ddc2b) == ddc1_ddc2b)
            description += " (supports DDC1 and DDC2B)";

        else if ((circuitOutput & ddc1) == ddc1) {
            description += " (supports DDC1";
            if ((circuitOutput & (1 << sense1)) == 0)
                description += "; SCL grounded";
            else if ((circuitOutput & (1 << sense1low_sense2)) == 0 && (circuitOutput & (1 << sense2low_sense1)) == 0)
                description += "; SCL/SDA linked";
            else {
                if ((circuitOutput & (1 << sense1low_sense2)) == 0)
                    description += "; SCL affects SDA";
                if ((circuitOutput & (1 << sense2low_sense1)) == 0)
                    description += "; SDA affects SCL";
            }
            description += ")";
        }
    }

    return description;
}

std::array<CircuitOutput , 512> senseCircuits;         // ARRAY[CircuitDiagram] OF CircuitOutput (sorted by CircuitDiagram)
std::array<CircuitDiagram, 512> senseCircuitsByOutput; // ARRAY[512] OF CircuitDiagram (sorted by output and then complexity)

SenseConnection Reverse(SenseConnection s10) {
    // transforms s01 connection to s10 connection
    static const SenseConnection reverseConnections[] = {
        connectionOpen, connectionForward, connectionBackward, connectionBidirectional
    };
    return reverseConnections[s10];
}

SenseLevel CalcValue(
    SenseLevel s0, SenseLevel s1, SenseLevel s2,
    SenseConnection s21, SenseConnection s20, SenseConnection s10
) {
    if (
        (s0 == levelLow) // s0 is grounded
        || (
            (s1 == levelLow) && ( // s1 is grounded and
                (s10 >= connectionForward) || // there's a link from s1 to s0 or
                ((Reverse(s21) >= connectionForward) && (s20 >= connectionForward)) // there's a link from s1 to s2 and s2 to s0
            )
        )
        || (
            (s2 == levelLow) && ( // s2 is grounded and
                (s20 >= connectionForward) || // there's a link from s2 to s0 or
                ((s21 >= connectionForward) && (s10 >= connectionForward)) // there's a link from s2 to s1 and s1 to s0
            )
        )
    )
        return levelLow;
    return levelHigh;
}

int CountDiodes(CircuitDiagram circuit) {
    return
        (BTST(circuit, connection21) != BTST(circuit, connection21 + 1)) +
        (BTST(circuit, connection20) != BTST(circuit, connection20 + 1)) +
        (BTST(circuit, connection10) != BTST(circuit, connection10 + 1)) ;
}

int CountStraightWires(CircuitDiagram circuit) {
    return
        (BAND(BSR(circuit, connection21), connectionMask) == connectionBidirectional) +
        (BAND(BSR(circuit, connection20), connectionMask) == connectionBidirectional) +
        (BAND(BSR(circuit, connection10), connectionMask) == connectionBidirectional) ;
}

int CountGrounds(CircuitDiagram circuit) {
    return 3 - (BTST(circuit, connection2)) - (BTST(circuit, connection1)) - (BTST(circuit, connection0));
}

bool CompareSenseOutput(CircuitDiagram circuit, CircuitDiagram circuit2) {
    int result;
    result = (int)senseCircuits[circuit] - (int)senseCircuits[circuit2]; // compare outputs of circuits
    if (result == 0) { // compare circuits
        result = CountDiodes(circuit) - CountDiodes(circuit2);
        if (result == 0) {
            result = CountStraightWires(circuit) + CountGrounds(circuit) - CountStraightWires(circuit2) - CountGrounds(circuit2);
            if (result == 0) {
                result = CountStraightWires(circuit) - CountStraightWires(circuit2);
                if (result == 0)
                    result = circuit - circuit2;
            }
        }
    }
    return result < 0;
}

std::string MakeCircuitOutputString (CircuitOutput circuitOutput) {
    char buf[22];
    snprintf(buf, sizeof(buf), "%d  %d  %d    %d%d  %d%d  %d%d",
        BTST(circuitOutput, sense2),
        BTST(circuitOutput, sense1),
        BTST(circuitOutput, sense0),
        BTST(circuitOutput, sense2low_sense1),
        BTST(circuitOutput, sense2low_sense0),
        BTST(circuitOutput, sense1low_sense2),
        BTST(circuitOutput, sense1low_sense0),
        BTST(circuitOutput, sense0low_sense2),
        BTST(circuitOutput, sense0low_sense1)
    );
    return buf;
}

int MakeMonitorSenseLines() {
    SenseLevel s0, s1, s2;
    SenseConnection s21, s20, s10;
    CircuitDiagram circuit = 0;
    for (s2 = levelLow; s2 <= levelHigh; s2 = SenseLevel(int(s2) + 1))
    for (s1 = levelLow; s1 <= levelHigh; s1 = SenseLevel(int(s1) + 1))
    for (s0 = levelLow; s0 <= levelHigh; s0 = SenseLevel(int(s0) + 1))
        for (s21 = connectionOpen; s21 <= connectionBidirectional; s21 = SenseConnection(int(s21) + 1))
        for (s20 = connectionOpen; s20 <= connectionBidirectional; s20 = SenseConnection(int(s20) + 1))
        for (s10 = connectionOpen; s10 <= connectionBidirectional; s10 = SenseConnection(int(s10) + 1))
        {
            senseCircuits[circuit] = 0
                | (CalcValue(s2, s0      , s1      , s10, Reverse(s21), Reverse(s20)) << sense2)
                | (CalcValue(s1, s0      , s2      , s20,         s21 , Reverse(s10)) << sense1)
                | (CalcValue(s0, s1      , s2      , s21,         s20 ,         s10 ) << sense0)
      
                | (CalcValue(s1, s0      , levelLow, s20,         s21 , Reverse(s10)) << sense2low_sense1)
                | (CalcValue(s0, s1      , levelLow, s21,         s20 ,         s10)  << sense2low_sense0)
      
                | (CalcValue(s2, s0      , levelLow, s10, Reverse(s21), Reverse(s20)) << sense1low_sense2)
                | (CalcValue(s0, levelLow, s2      , s21,         s20 ,         s10)  << sense1low_sense0)

                | (CalcValue(s2, levelLow, s1      , s10, Reverse(s21), Reverse(s20)) << sense0low_sense2)
                | (CalcValue(s1, levelLow, s2      , s20,         s21 , Reverse(s10)) << sense0low_sense1)
            ;
            #if 0
                printf("%s = %s\n",
                    MakeCircuitOutputString(circuit).c_str(),
                    MakeCircuitOutputString(senseCircuits[circuit]).c_str()
                );
            #endif
            circuit++;
        }
    for (circuit = 0; circuit < 512; circuit++)
        senseCircuitsByOutput[circuit] = circuit;
    std::sort(senseCircuitsByOutput.begin(), senseCircuitsByOutput.end(), CompareSenseOutput);
    return 1;
}

void DrawCircuits() {
    const int kSpaceBetween = 3;
    const int kWidth = 7;
    const int kTotalWidth = kWidth + kSpaceBetween;

    const int kLeftMargin = 3;
    const int kRightMargin = 0;

    const int kFirstRowCircuits = 4; // First row has fiewer circuits so that you can add text to the right side of the row.
    const int kMaxCircuits = 400; // 4 suggested, 13 for whole width, ≥ 304 for all

    const int portRect_right = 80;
    char printlines[3][portRect_right + 1]; // Add one for terminating null character.

    int tableNdx;
    CircuitOutput circuitOutput;

    int curCircuitOutput;
    CircuitDiagram circuit;
    int curOffset = kLeftMargin;

    bool isNewLine = true;
    std::string outputString;
    std::string outputString2;

    int outputCount;
    int sameOutputCount = 0;

    int circuitCount = 0;

    outputCount = 0;
    curCircuitOutput = 0xffff;
    for (tableNdx = 0; tableNdx <= 512; tableNdx++)
    {
        if (tableNdx < 512) {
            circuit = senseCircuitsByOutput[tableNdx];
            circuitOutput = senseCircuits[circuit];
        } else {
            circuit = 0xffff;
            circuitOutput = 0;
        }

        if (circuitOutput == curCircuitOutput) {
            if (circuitCount <= kMaxCircuits)
                curOffset = curOffset + kTotalWidth;
            if ((curOffset > portRect_right - kWidth - kRightMargin) ||
                ((circuitCount == kFirstRowCircuits + 1) && (circuitCount <= kMaxCircuits))
            ) {
                for (int line = 0; line < 3; line++)
                    printf("%s\n", printlines[line]);
                printf("\n");
                curOffset = kLeftMargin;
                memset(printlines, ' ', sizeof(printlines));
                for (int line = 0; line < 3; line++)
                    printlines[line][portRect_right] = '\0';
            }
        }
        else {
            if (curCircuitOutput != 0xffff)
                for (int line = 0; line < 3; line++)
                    printf("%s\n", printlines[line]);

            curOffset = kLeftMargin;
            memset(printlines, ' ', sizeof(printlines));
            for (int line = 0; line < 3; line++)
                printlines[line][portRect_right] = '\0';

            if (BSR(circuitOutput, 6) != BSR(curCircuitOutput, 6))
                printf("================================================================================\n");
            else
                printf("--------------------------------------------------------------------------------\n");

            if (tableNdx == 512)
                break;

            circuitCount = 1;
            outputCount++;
            sameOutputCount = tableNdx + 1;
            while ((sameOutputCount < 512) && (circuitOutput == senseCircuits[senseCircuitsByOutput[sameOutputCount]]))
                sameOutputCount++;
            sameOutputCount = sameOutputCount - tableNdx;

            isNewLine = true;

            curCircuitOutput = circuitOutput;
        }

        if (isNewLine) {
            isNewLine = false;
            outputString = MakeCircuitOutputString(circuitOutput);
            outputString2 = MonitorCodeToDescription(circuitOutput);
            printf("%2d)   [ %s ] %*s(x%d)    %s\n", outputCount, outputString.c_str(),
                (sameOutputCount < 10) + (sameOutputCount < 100), "", sameOutputCount, outputString2.c_str());
        }

        if (circuitCount <= kMaxCircuits) {
            printlines[2][curOffset+1] = '2';
            printlines[0][curOffset+3] = '1';
            printlines[2][curOffset+5] = '0';

            printlines[2][curOffset+0] = "g "[BTST(circuit, connection2)];
            printlines[0][curOffset+4] = "g "[BTST(circuit, connection1)];
            printlines[2][curOffset+6] = "g "[BTST(circuit, connection0)];

            printlines[1][curOffset+2] = " ^v/" [BAND(BSR(circuit, connection21), connectionMask)];
            printlines[2][curOffset+3] = " ><-" [BAND(BSR(circuit, connection20), connectionMask)];
            printlines[1][curOffset+4] = " v^\\"[BAND(BSR(circuit, connection10), connectionMask)];
        }
        circuitCount++;
    } // FOR

    printf("512 different circuits producing %d different outputs.\n", outputCount);
    printf("Description Line: [ Sense 2  Sense 1  Sense 0  1-0(2 low)  2-0(1 low)  2-1(0 low) ]  (x No. of circuits)\n");
    printf("================================================================================\n");
}

bool MonitorSenseLines() {
    MakeMonitorSenseLines();
    //DrawCircuits();
    return 1;
}

static bool DoneMonitorSenseLines = MonitorSenseLines();
