//  ---------------------------------------------------------------------------
//  This file is part of reSID, a MOS6581 SID emulator engine.
//  Copyright (C) 2010  Dag Lem <resid@nimrod.no>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  ---------------------------------------------------------------------------

#ifndef RESID_FILTER_H
#define RESID_FILTER_H

#include "resid-config.h"

#include <cassert>
#include <cstdlib>

namespace reSID
{

// ----------------------------------------------------------------------------
// The SID filter is modeled with a two-integrator-loop biquadratic filter,
// which has been confirmed by Bob Yannes to be the actual circuit used in
// the SID chip.
//
// Measurements show that excellent emulation of the SID filter is achieved,
// except when high resonance is combined with high sustain levels.
// In this case the SID op-amps are performing less than ideally and are
// causing some peculiar behavior of the SID filter. This however seems to
// have more effect on the overall amplitude than on the color of the sound.
//
// The theory for the filter circuit can be found in "Microelectric Circuits"
// by Adel S. Sedra and Kenneth C. Smith.
// The circuit is modeled based on the explanation found there except that
// an additional inverter is used in the feedback from the bandpass output,
// allowing the summer op-amp to operate in single-ended mode. This yields
// filter outputs with levels independent of Q, which corresponds with the
// results obtained from a real SID.
//
// We have been able to model the summer and the two integrators of the circuit
// to form components of an IIR filter.
// Vhp is the output of the summer, Vbp is the output of the first integrator,
// and Vlp is the output of the second integrator in the filter circuit.
//
// According to Bob Yannes, the active stages of the SID filter are not really
// op-amps. Rather, simple NMOS inverters are used. By biasing an inverter
// into its region of quasi-linear operation using a feedback resistor from
// input to output, a MOS inverter can be made to act like an op-amp for
// small signals centered around the switching threshold.
//
// In 2008, Michael Huth facilitated closer investigation of the SID 6581
// filter circuit by publishing high quality microscope photographs of the die.
// Tommi Lempinen has done an impressive work on re-vectorizing and annotating
// the die photographs, substantially simplifying further analysis of the
// filter circuit.
//
// The filter schematics below are reverse engineered from these re-vectorized
// and annotated die photographs. While the filter first depicted in reSID 0.9
// is a correct model of the basic filter, the schematics are now completed
// with the audio mixer and output stage, including details on intended
// relative resistor values. Also included are schematics for the NMOS FET
// voltage controlled resistors (VCRs) used to control cutoff frequency, the
// DAC which controls the VCRs, the NMOS op-amps, and the output buffer.
//
//
// SID 6581 filter / mixer / output
// --------------------------------
//
//                ---------------------------------------------------
//               |                                                   |
//               |                         --1R1-- \--  D7           |
//               |              ---R1--   |           |              |
//               |             |       |  |--2R1-- \--| D6           |
//               |    ------------<A]-----|           |     $17      |
//               |   |                    |--4R1-- \--| D5  1=open   | (3.5R1)
//               |   |                    |           |              |
//               |   |                     --8R1-- \--| D4           | (7.0R1)
//               |   |                                |              |
// $17           |   |                    (CAP2B)     |  (CAP1B)     |
// 0=to mixer    |    --R8--    ---R8--        ---C---|       ---C---|
// 1=to filter   |          |  |       |      |       |      |       |
//                ------R8--|-----[A>--|--Rw-----[A>--|--Rw-----[A>--|
//     ve (EXT IN)          |          |              |              |
// D3  \ ---------------R8--|          |              | (CAP2A)      | (CAP1A)
//     |   v3               |          | vhp          | vbp          | vlp
// D2  |   \ -----------R8--|     -----               |              |
//     |   |   v2           |    |                    |              |
// D1  |   |   \ -------R8--|    |    ----------------               |
//     |   |   |   v1       |    |   |                               |
// D0  |   |   |   \ ---R8--     |   |    ---------------------------
//     |   |   |   |             |   |   |
//     R6  R6  R6  R6            R6  R6  R6
//     |   |   |   | $18         |   |   |  $18
//     |    \  |   | D7: 1=open   \   \   \ D6 - D4: 0=open
//     |   |   |   |             |   |   |
//      ---------------------------------                          12V
//                 |
//                 |               D3  --/ --1R4--                  |
//                 |    ---R8--       |           |   ---R3--       |
//                 |   |       |   D2 |--/ --2R4--|  |       |  ||--
//                  ------[A>---------|           |-----[A>-----||
//                                 D1 |--/ --4R4--| (4.25R2)    ||--
//                        $18         |           |                 |
//                        0=open   D0  --/ --8R4--  (8.75R2)        |
//
//                                                                  vo (AUDIO
//                                                                      OUT)
//
//
// v1  - voice 1
// v2  - voice 2
// v3  - voice 3
// ve  - ext in
// vhp - highpass output
// vbp - bandpass output
// vlp - lowpass output
// vo  - audio out
// [A> - single ended inverting op-amp (self-biased NMOS inverter)
// Rn  - "resistors", implemented with custom NMOS FETs
// Rw  - cutoff frequency resistor (VCR)
// C   - capacitor
//
// Notes:
//
// R2  ~  2.0*R1
// R6  ~  6.0*R1
// R8  ~  8.0*R1
// R24 ~ 24.0*R1
//
// The Rn "resistors" in the circuit are implemented with custom NMOS FETs,
// probably because of space constraints on the SID die. The silicon substrate
// is laid out in a narrow strip or "snake", with a strip length proportional
// to the intended resistance. The polysilicon gate electrode covers the entire
// silicon substrate and is fixed at 12V in order for the NMOS FET to operate
// in triode mode (a.k.a. linear mode or ohmic mode).
//
// Even in "linear mode", an NMOS FET is only an approximation of a resistor,
// as the apparant resistance increases with increasing drain-to-source
// voltage. If the drain-to-source voltage should approach the gate voltage
// of 12V, the NMOS FET will enter saturation mode (a.k.a. active mode), and
// the NMOS FET will not operate anywhere like a resistor.
//
//
//
// NMOS FET voltage controlled resistor (VCR)
// ------------------------------------------
//
//                Vw
//
//                |
//                |
//                R1
//                |
//          --R1--|
//         |    __|__
//         |    -----
//         |    |   |
// vi ----------     -------- vo
//         |           |
//          ----R24----
//
//
// vi  - input
// vo  - output
// Rn  - "resistors", implemented with custom NMOS FETs
// Vw  - voltage from 11-bit DAC (frequency cutoff control)
//
// Notes:
//
// An approximate value for R24 can be found by using the formula for the
// filter cutoff frequency:
//
// FCmin = 1/(2*pi*Rmax*C)
//
// Assuming that a the setting for minimum cutoff frequency in combination with
// a low level input signal ensures that only negligible current will flow
// through the transistor in the schematics above, values for FCmin and C can
// be substituted in this formula to find Rmax.
// Using C = 470pF and FCmin = 220Hz (measured value), we get:
//
// FCmin = 1/(2*pi*Rmax*C)
// Rmax = 1/(2*pi*FCmin*C) = 1/(2*pi*220*470e-12) ~ 1.5MOhm
//
// From this it follows that:
// R24 =  Rmax   ~ 1.5MOhm
// R1  ~  R24/24 ~  64kOhm
// R2  ~  2.0*R1 ~ 128kOhm
// R6  ~  6.0*R1 ~ 384kOhm
// R8  ~  8.0*R1 ~ 512kOhm
//
// Note that these are only approximate values for one particular SID chip,
// due to process variations the values can be substantially different in
// other chips.
//
//
//
// Filter frequency cutoff DAC
// ---------------------------
//
//
//        12V  10   9   8   7   6   5   4   3   2   1   0   VGND
//          |   |   |   |   |   |   |   |   |   |   |   |     |   Missing
//         2R  2R  2R  2R  2R  2R  2R  2R  2R  2R  2R  2R    2R   termination
//          |   |   |   |   |   |   |   |   |   |   |   |     |
//     Vw ----R---R---R---R---R---R---R---R---R---R---R--   ---
//
// Bit on:  12V
// Bit off:  5V (VGND)
//
// As is the case with all MOS 6581 DACs, the termination to (virtual) ground
// at bit 0 is missing.
//
// Furthermore, the control of the two VCRs imposes a load on the DAC output
// which varies with the input signals to the VCRs. This can be seen from the
// VCR figure above.
//
//
//
// "Op-amp" (self-biased NMOS inverter)
// ------------------------------------
//
//
//                        12V
//
//                         |
//              -----------|
//             |           |
//             |     ------|
//             |    |      |
//             |    |  ||--
//             |     --||
//             |       ||--
//         ||--            |
// vi -----||              |--------- vo
//         ||--            |   |
//             |       ||--    |
//             |-------||      |
//             |       ||--    |
//         ||--            |   |
//       --||              |   |
//      |  ||--            |   |
//      |      |           |   |
//      |       -----------|   |
//      |                  |   |
//      |                      |
//      |                 GND  |
//      |                      |
//       ----------------------
//
//
// vi  - input
// vo  - output
//
// Notes:
//
// The schematics above are laid out to show that the "op-amp" logically
// consists of two building blocks; a saturated load NMOS inverter (on the
// right hand side of the schematics) with a buffer / bias input stage
// consisting of a variable saturated load NMOS inverter (on the left hand
// side of the schematics).
//
// Provided a reasonably high input impedance and a reasonably low output
// impedance, the "op-amp" can be modeled as a voltage transfer function
// mapping input voltage to output voltage.
//
//
//
// Output buffer (NMOS voltage follower)
// -------------------------------------
//
//
//            12V
//
//             |
//             |
//         ||--
// vi -----||
//         ||--
//             |
//             |------ vo
//             |     (AUDIO
//            Rext    OUT)
//             |
//             |
//
//            GND
//
// vi   - input
// vo   - output
// Rext - external resistor, 1kOhm
//
// Notes:
//
// The external resistor Rext is needed to complete the NMOS voltage follower,
// this resistor has a recommended value of 1kOhm.
//
// Die photographs show that actually, two NMOS transistors are used in the
// voltage follower. However the two transistors are coupled in parallel (all
// terminals are pairwise common), which implies that we can model the two
// transistors as one.
//
// ----------------------------------------------------------------------------
//
// SID 8580 filter / mixer / output
// --------------------------------
//
//               +---------------------------------------------------+
//               |    $17      +----Rf-+                             |
//               |             |       |                             |
//               |      D4&!D5 o- \-R3-o                             |
//               |             |       |                    $17      |
//               |     !D4&!D5 o- \-R2-o                             |
//               |             |       |  +---R8-- \--+  !D6&D7      |
//               |      D4&!D5 o- \-R1-o  |           |              |
//               |             |       |  o---RC-- \--o   D6&D7      |
//               |   +---------o--<A]--o--o           |              |
//               |   |                    o---R4-- \--o  D6&!D7      |
//               |   |                    |           |              |
//               |   |                    +---Ri-- \--o !D6&!D7      |
//               |   |                                |              |
// $17           |   |                    (CAP2B)     |  (CAP1B)     |
// 0=to mixer    |   +--R7--+  +---R7--+      +---C---o      +---C---o
// 1=to filter   |          |  |       |      |       |      |       |
//               +------R7--o--o--[A>--o--Rfc-o--[A>--o--Rfc-o--[A>--o
//     ve (EXT IN)          |          |              |              |
// D3  \ --------------R12--o          |              | (CAP2A)      | (CAP1A)
//     |   v3               |          | vhp          | vbp          | vlp
// D2  |   \ -----------R7--o    +-----+              |              |
//     |   |   v2           |    |                    |              |
// D1  |   |   \ -------R7--o    |   +----------------+              |
//     |   |   |   v1       |    |   |                               |
// D0  |   |   |   \ ---R7--+    |   |   +---------------------------+
//     |   |   |   |             |   |   |
//     R9  R5  R5  R5            R5  R5  R5
//     |   |   |   | $18         |   |   |  $18
//     |    \  |   | D7: 1=open   \   \   \ D6 - D4: 0=open
//     |   |   |   |             |   |   |
//     +---o---o---o-------------o---o---+
//                 |
//                 |               D3 +--/ --1R4--+
//                 |   +---R8--+      |           |  +---R2--+
//                 |   |       |   D2 o--/ --2R4--o  |       |
//                 +---o--[A>--o------o           o--o--[A>--o-- vo (AUDIO OUT)
//                                 D1 o--/ --4R4--o
//                        $18         |           |
//                        0=open   D0 +--/ --8R4--+
//
//
//
//
// R1 = 15.3*Ri
// R2 =  7.3*Ri
// R3 =  4.7*Ri
// Rf =  1.4*Ri
// R4 =  1.4*Ri
// R8 =  2.0*Ri
// RC =  2.8*Ri
//
//
//
// Op-amps
// -------
// Unlike the 6581, the 8580 has real OpAmps.
//
// Temperature compensated differential amplifier:
//
//                9V
//
//                |
//      +-------o-o-o-------+
//      |       |   |       |
//      |       R   R       |
//      +--||   |   |   ||--+
//         ||---o   o---||
//      +--||   |   |   ||--+
//      |       |   |       |
//      o-----+ |   |       o--- Va
//      |     | |   |       |
//      +--|| | |   |   ||--+
//         ||-o-+---+---||
//      +--||   |   |   ||--+
//      |       |   |       |
//              |   |
//     GND      |   |      GND
//          ||--+   +--||
// in- -----||         ||------ in+
//          ||----o----||
//                |
//                8 Current sink
//                |
//
//               GND
//
// Inverter + non-inverting output amplifier:
//
// Va ---o---||-------------------o--------------------+
//       |                        |               9V   |
//       |             +----------+----------+     |   |
//       |        9V   |          |     9V   | ||--+   |
//       |         |   |      9V  |      |   +-||      |
//       |         R   |       |  |  ||--+     ||--+   |
//       |         |   |   ||--+  +--||            o---o--- Vout
//       |         o---o---||        ||--+     ||--+
//       |         |       ||--+         o-----||
//       |     ||--+           |     ||--+     ||--+
//       +-----||              o-----||            |
//             ||--+           |     ||--+
//                 |           R         |        GND
//                             |
//                GND                   GND
//                            GND
//
//
//
// Virtual ground
// --------------
// A PolySi resitive voltage divider provides the voltage
// for the non-inverting input of the filter op-amps.
//
//     5V
//          +----------+
//      |   |   |\     |
//      R1  +---|-\    |
// 5V   |       |A >---o--- Vref
//      o-------|+/
//  |   |       |/
// R10  R4
//  |   |
//  o---+
//  |
// R10
//  |
//
// GND
//
// Rn = n*R1
//
//
//
// Rfc - freq control DAC resistance ladder
// ----------------------------------------
// The resistance for the bandpass and lowpass integrator stages of the filter are determined
// by an 11 bit DAC driven by the FC register.
// If all 11 bits are '0', the impedance of the DAC would be "infinitely high".
// To get around this, there is an 11 input NOR gate below the DAC sensing those 11 bits.
// If they are all 0, the NOR gate gives the gate control voltage to the 12 bit DAC LSB.
//
//
//
// Crystal stabilized precision switched capacitor voltage divider
// ---------------------------------------------------------------
// There is a FET working as a temperature sensor close to the DACs which changes the gate voltage
// of the frequency control DACs according to the temperature, to reduce its effects on the filter curve.
// An asynchronous 3 bit binary counter, running at the speed of PHI2, drives two big capacitors
// which AC resistance is then used as a voltage divider.
// This implicates that frequency difference between PAL and NTSC might shift the filter curve by 4% or such.
//
// https://en.wikipedia.org/wiki/Switched_capacitor
//
//                                |\  OpAmp has a smaller capacitor
//                        Vref ---|+\            than the other OPs
//                                |A >---o--- Vdac
//                        o-------|-/    |
//                        |       |/     |
//                        |              |
//       C1               |     C2       |
//   +---||---o---+   +---o-----||-------o
//   |        |   |   |   |              |
//   o----+   |   -----   |              |
//   |    |   |   -----   +----+   +-----+
//   |    -----     |          |   |     |
//   |    -----     |          -----     |
//   |      |       |          -----     |
//   |    +-----------+          |       |
//        | /Q      Q |          +-------+
//  GND   +-----------+      FET close to DAC
//        |   clk/8   |      working as temperature sensor
//        +-----------+
//          |       |
//         clk1    clk2
//

// Compile-time computation of op-amp summer and mixer table offsets.

// The highpass summer has 2 - 6 inputs (bandpass, lowpass, and 0 - 4 voices).
template<int i>
struct summer_offset
{
  enum { value = summer_offset<i - 1>::value + ((2 + i - 1) << 16) };
};

template<>
struct summer_offset<0>
{
  enum { value = 0 };
};

// The mixer has 0 - 7 inputs (0 - 4 voices and 0 - 3 filter outputs).
template<int i>
struct mixer_offset
{
  enum { value = mixer_offset<i - 1>::value + ((i - 1) << 16) };
};

template<>
struct mixer_offset<1>
{
  enum { value = 1 };
};

template<>
struct mixer_offset<0>
{
  enum { value = 0 };
};


class Filter
{
public:
  Filter();

  void enable_filter(bool enable);
  void adjust_filter_bias(double dac_bias);
  void set_chip_model(chip_model model);
  void set_voice_mask(reg4 mask);

  void clock(int voice1, int voice2, int voice3);
  void clock(cycle_count delta_t, int voice1, int voice2, int voice3);
  void reset();

  // Write registers.
  void writeFC_LO(reg8);
  void writeFC_HI(reg8);
  void writeRES_FILT(reg8);
  void writeMODE_VOL(reg8);

  // SID audio input (16 bits).
  void input(short sample);

  // SID audio output (16 bits).
  short output();

protected:
  void set_sum_mix();
  void set_w0();

  // Filter enabled.
  bool enabled;

  // Filter cutoff frequency.
  reg12 fc;

  // Filter resonance.
  reg8 res;

  // Selects which voices to route through the filter.
  reg8 filt;

  // Selects which filter outputs to route into the mixer.
  reg4 mode;

  // Output master volume.
  reg4 vol;

  // Used to mask out EXT IN if not connected, and for test purposes
  // (voice muting).
  reg8 voice_mask;

  // Select which inputs to route into the summer / mixer.
  // These are derived from filt, mode, and voice_mask.
  reg8 sum;
  reg8 mix;

  // State of filter.
  int Vhp; // highpass
  int Vbp; // bandpass
  int Vbp_x, Vbp_vc;
  int Vlp; // lowpass
  int Vlp_x, Vlp_vc;
  // Filter / mixer inputs.
  int ve;
  int v3;
  int v2;
  int v1;

  chip_model sid_model;

  typedef struct {
    unsigned short vx;
    short dvx;
  } opamp_t;

  typedef struct {
    int kVddt;   // K*(Vdd - Vth)
    int voice_scale_s14;
    int voice_DC;
    int ak;
    int bk;
    int vc_min;
    int vc_max;
    int filterGain;
    double vo_N16;  // Fixed point scaling for 16 bit op-amp output.

    // Reverse op-amp transfer function.
    unsigned short opamp_rev[1 << 16];
    // Lookup tables for gain and summer op-amps in output stage / filter.
    unsigned short summer[summer_offset<5>::value];
    unsigned short gain[16][1 << 16];
    unsigned short resonance[16][1 << 16];
    unsigned short mixer[mixer_offset<8>::value];
    // Cutoff frequency DAC output voltage table. FC is an 11 bit register.
    unsigned short f0_dac[1 << 11];
  } model_filter_t;

  // 6581 only
  // Cutoff frequency DAC voltage, resonance.
  int Vddt_Vw_2, Vw_bias;

  static int n_snake;

  // 8580 only
  int n_dac;

  static int n_param;

  // DAC gate voltage
  int nVgt;

  //int solve_gain(opamp_t* opamp, int n, int vi_t, int& x, model_filter_t& mf);
  int solve_gain_d(opamp_t* opamp, double n, int vi_t, int& x, model_filter_t& mf);
  int solve_integrate_6581(int dt, int vi_t, int& x, int& vc, model_filter_t& mf);
  int solve_integrate_8580(int dt, int vi_t, int& x, int& vc, model_filter_t& mf);

  // VCR - 6581 only.
  static unsigned short vcr_kVg[1 << 16];
  static unsigned short vcr_n_Ids_term[1 << 16];
  // Common parameters.
  static model_filter_t model_filter[2];

friend class SID;

private:
    class Randomnoise
    {
    private:
        int buffer[1024];
        mutable int index = 0;
    public:
        Randomnoise()
        {
            for (int i=0; i<1024; i++)
                buffer[i] = rand() % (1<<19);
        }
        int getNoise() const { index = (index + 1) & 0x3ff; return buffer[index]; }
    };

    Randomnoise rnd;
};


// ----------------------------------------------------------------------------
// Inline functions.
// The following functions are defined inline because they are called every
// time a sample is calculated.
// ----------------------------------------------------------------------------

#if RESID_INLINING || defined(RESID_FILTER_CC)

// ----------------------------------------------------------------------------
// SID clocking - 1 cycle.
// ----------------------------------------------------------------------------
RESID_INLINE
void Filter::clock(int voice1, int voice2, int voice3)
{
  model_filter_t& f = model_filter[sid_model];

  v1 = ((voice1*f.voice_scale_s14 + rnd.getNoise()) >> 18) + f.voice_DC;
  v2 = ((voice2*f.voice_scale_s14 + rnd.getNoise()) >> 18) + f.voice_DC;
  v3 = ((voice3*f.voice_scale_s14 + rnd.getNoise()) >> 18) + f.voice_DC;

  // Sum inputs routed into the filter.
  int Vi = 0;
  int offset = 0;

  switch (sum & 0xf) {
  case 0x0:
    Vi = 0;
    offset = summer_offset<0>::value;
    break;
  case 0x1:
    Vi = v1;
    offset = summer_offset<1>::value;
    break;
  case 0x2:
    Vi = v2;
    offset = summer_offset<1>::value;
    break;
  case 0x3:
    Vi = v2 + v1;
    offset = summer_offset<2>::value;
    break;
  case 0x4:
    Vi = v3;
    offset = summer_offset<1>::value;
    break;
  case 0x5:
    Vi = v3 + v1;
    offset = summer_offset<2>::value;
    break;
  case 0x6:
    Vi = v3 + v2;
    offset = summer_offset<2>::value;
    break;
  case 0x7:
    Vi = v3 + v2 + v1;
    offset = summer_offset<3>::value;
    break;
  case 0x8:
    Vi = ve;
    offset = summer_offset<1>::value;
    break;
  case 0x9:
    Vi = ve + v1;
    offset = summer_offset<2>::value;
    break;
  case 0xa:
    Vi = ve + v2;
    offset = summer_offset<2>::value;
    break;
  case 0xb:
    Vi = ve + v2 + v1;
    offset = summer_offset<3>::value;
    break;
  case 0xc:
    Vi = ve + v3;
    offset = summer_offset<2>::value;
    break;
  case 0xd:
    Vi = ve + v3 + v1;
    offset = summer_offset<3>::value;
    break;
  case 0xe:
    Vi = ve + v3 + v2;
    offset = summer_offset<3>::value;
    break;
  case 0xf:
    Vi = ve + v3 + v2 + v1;
    offset = summer_offset<4>::value;
    break;
  }

  // Calculate filter outputs.
  if (sid_model == 0) {
    // MOS 6581.
    Vlp = solve_integrate_6581(1, Vbp, Vlp_x, Vlp_vc, f);
    Vbp = solve_integrate_6581(1, Vhp, Vbp_x, Vbp_vc, f);
  }
  else {
    // MOS 8580.
    Vlp = solve_integrate_8580(1, Vbp, Vlp_x, Vlp_vc, f);
    Vbp = solve_integrate_8580(1, Vhp, Vbp_x, Vbp_vc, f);
  }

  assert((Vbp >= 0) && (Vbp < (1 << 16)));
  const int idx = offset + f.resonance[res][Vbp] + Vlp + Vi;
  assert((idx >= 0) && (idx < summer_offset<5>::value));
  Vhp = f.summer[idx];
}

// ----------------------------------------------------------------------------
// SID clocking - delta_t cycles.
// ----------------------------------------------------------------------------
RESID_INLINE
void Filter::clock(cycle_count delta_t, int voice1, int voice2, int voice3)
{
  model_filter_t& f = model_filter[sid_model];

  v1 = ((voice1*f.voice_scale_s14 + rnd.getNoise()) >> 18) + f.voice_DC;
  v2 = ((voice2*f.voice_scale_s14 + rnd.getNoise()) >> 18) + f.voice_DC;
  v3 = ((voice3*f.voice_scale_s14 + rnd.getNoise()) >> 18) + f.voice_DC;

  // Enable filter on/off.
  // This is not really part of SID, but is useful for testing.
  // On slow CPUs it may be necessary to bypass the filter to lower the CPU
  // load.
  if (unlikely(!enabled)) {
    return;
  }

  // Sum inputs routed into the filter.
  int Vi = 0;
  int offset = 0;

  switch (sum & 0xf) {
  case 0x0:
    Vi = 0;
    offset = summer_offset<0>::value;
    break;
  case 0x1:
    Vi = v1;
    offset = summer_offset<1>::value;
    break;
  case 0x2:
    Vi = v2;
    offset = summer_offset<1>::value;
    break;
  case 0x3:
    Vi = v2 + v1;
    offset = summer_offset<2>::value;
    break;
  case 0x4:
    Vi = v3;
    offset = summer_offset<1>::value;
    break;
  case 0x5:
    Vi = v3 + v1;
    offset = summer_offset<2>::value;
    break;
  case 0x6:
    Vi = v3 + v2;
    offset = summer_offset<2>::value;
    break;
  case 0x7:
    Vi = v3 + v2 + v1;
    offset = summer_offset<3>::value;
    break;
  case 0x8:
    Vi = ve;
    offset = summer_offset<1>::value;
    break;
  case 0x9:
    Vi = ve + v1;
    offset = summer_offset<2>::value;
    break;
  case 0xa:
    Vi = ve + v2;
    offset = summer_offset<2>::value;
    break;
  case 0xb:
    Vi = ve + v2 + v1;
    offset = summer_offset<3>::value;
    break;
  case 0xc:
    Vi = ve + v3;
    offset = summer_offset<2>::value;
    break;
  case 0xd:
    Vi = ve + v3 + v1;
    offset = summer_offset<3>::value;
    break;
  case 0xe:
    Vi = ve + v3 + v2;
    offset = summer_offset<3>::value;
    break;
  case 0xf:
    Vi = ve + v3 + v2 + v1;
    offset = summer_offset<4>::value;
    break;
  }

  // Maximum delta cycles for filter fixpoint iteration to converge
  // is approximately 3.
  cycle_count delta_t_flt = 3;

  if (sid_model == 0) {
    // MOS 6581.
    while (delta_t) {
      if (unlikely(delta_t < delta_t_flt)) {
        delta_t_flt = delta_t;
      }

      // Calculate filter outputs.
      Vlp = solve_integrate_6581(delta_t_flt, Vbp, Vlp_x, Vlp_vc, f);
      Vbp = solve_integrate_6581(delta_t_flt, Vhp, Vbp_x, Vbp_vc, f);
      assert((Vbp >= 0) && (Vbp < (1 << 16)));
      const int idx = offset + f.resonance[res][Vbp] + Vlp + Vi;
      assert((idx >= 0) && (idx < summer_offset<5>::value));
      Vhp = f.summer[idx];

      delta_t -= delta_t_flt;
    }
  }
  else {
    // MOS 8580.
    while (delta_t) {
      if (unlikely(delta_t < delta_t_flt)) {
        delta_t_flt = delta_t;
      }

      // Calculate filter outputs.
      Vlp = solve_integrate_8580(delta_t_flt, Vbp, Vlp_x, Vlp_vc, f);
      Vbp = solve_integrate_8580(delta_t_flt, Vhp, Vbp_x, Vbp_vc, f);
      assert((Vbp >= 0) && (Vbp < (1 << 16)));
      const int idx = offset + f.resonance[res][Vbp] + Vlp + Vi;
      assert((idx >= 0) && (idx < summer_offset<5>::value));
      Vhp = f.summer[idx];

      delta_t -= delta_t_flt;
    }
  }
}


// ----------------------------------------------------------------------------
// SID audio input (16 bits).
// ----------------------------------------------------------------------------
RESID_INLINE
void Filter::input(short sample)
{
  // Scale to three times the peak-to-peak for one voice and add the op-amp
  // "zero" DC level.
  // NB! Adding the op-amp "zero" DC level is a (wildly inaccurate)
  // approximation of feeding the input through an AC coupling capacitor.
  // This could be implemented as a separate filter circuit, however the
  // primary use of the emulator is not to process external signals.
  // The upside is that the MOS8580 "digi boost" works without a separate (DC)
  // input interface.
  // Note that the input is 16 bits, compared to the 20 bit voice output.
  model_filter_t& f = model_filter[sid_model];
  ve = (sample*f.voice_scale_s14*3 >> 14) + f.mixer[0];
}


// ----------------------------------------------------------------------------
// SID audio output (16 bits).
// ----------------------------------------------------------------------------
RESID_INLINE
short Filter::output()
{
  model_filter_t& f = model_filter[sid_model];

  // Writing the switch below manually would be tedious and error-prone;
  // it is rather generated by the following Perl program:

  /*
my @i = qw(v1 v2 v3 ve Vlp Vbp Vhp);
for my $mix (0..2**@i-1) {
    print sprintf("  case 0x%02x:\n", $mix);
    my @sumVoice;
    my @sumFilt;
    my $bit = 0;
    for (@i) {
        if ($bit < 4) {
            unshift(@sumVoice, $_) if $mix & (1 << $bit);
        } else {
            unshift(@sumFilt, $_) if $mix & (1 << $bit);
        }
        $bit += 1;
    }
    my $sum;
    if (@sumFilt) {
        $sumV = (@sumVoice) ? " + ".join(" + ", @sumVoice) : "";
        $sum = "(((".join(" + ", @sumFilt).") * mf.filterGain) >> 12)" . $sumV;
    } else {
        $sum = join(" + ", @sumVoice) || "0";
    }
    print "    Vi = $sum;\n";
    print "    offset = mixer_offset<" . (@sumVoice+@sumFilt) . ">::value;\n";
    print "    break;\n";
}
  */

  // Sum inputs routed into the mixer.
  int Vi = 0;
  int offset = 0;

  switch (mix & 0x7f) {
  case 0x00:
    Vi = 0;
    offset = mixer_offset<0>::value;
    break;
  case 0x01:
    Vi = v1;
    offset = mixer_offset<1>::value;
    break;
  case 0x02:
    Vi = v2;
    offset = mixer_offset<1>::value;
    break;
  case 0x03:
    Vi = v2 + v1;
    offset = mixer_offset<2>::value;
    break;
  case 0x04:
    Vi = v3;
    offset = mixer_offset<1>::value;
    break;
  case 0x05:
    Vi = v3 + v1;
    offset = mixer_offset<2>::value;
    break;
  case 0x06:
    Vi = v3 + v2;
    offset = mixer_offset<2>::value;
    break;
  case 0x07:
    Vi = v3 + v2 + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x08:
    Vi = ve;
    offset = mixer_offset<1>::value;
    break;
  case 0x09:
    Vi = ve + v1;
    offset = mixer_offset<2>::value;
    break;
  case 0x0a:
    Vi = ve + v2;
    offset = mixer_offset<2>::value;
    break;
  case 0x0b:
    Vi = ve + v2 + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x0c:
    Vi = ve + v3;
    offset = mixer_offset<2>::value;
    break;
  case 0x0d:
    Vi = ve + v3 + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x0e:
    Vi = ve + v3 + v2;
    offset = mixer_offset<3>::value;
    break;
  case 0x0f:
    Vi = ve + v3 + v2 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x10:
    Vi = (((Vlp) * f.filterGain) >> 12);
    offset = mixer_offset<1>::value;
    break;
  case 0x11:
    Vi = (((Vlp) * f.filterGain) >> 12) + v1;
    offset = mixer_offset<2>::value;
    break;
  case 0x12:
    Vi = (((Vlp) * f.filterGain) >> 12) + v2;
    offset = mixer_offset<2>::value;
    break;
  case 0x13:
    Vi = (((Vlp) * f.filterGain) >> 12) + v2 + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x14:
    Vi = (((Vlp) * f.filterGain) >> 12) + v3;
    offset = mixer_offset<2>::value;
    break;
  case 0x15:
    Vi = (((Vlp) * f.filterGain) >> 12) + v3 + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x16:
    Vi = (((Vlp) * f.filterGain) >> 12) + v3 + v2;
    offset = mixer_offset<3>::value;
    break;
  case 0x17:
    Vi = (((Vlp) * f.filterGain) >> 12) + v3 + v2 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x18:
    Vi = (((Vlp) * f.filterGain) >> 12) + ve;
    offset = mixer_offset<2>::value;
    break;
  case 0x19:
    Vi = (((Vlp) * f.filterGain) >> 12) + ve + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x1a:
    Vi = (((Vlp) * f.filterGain) >> 12) + ve + v2;
    offset = mixer_offset<3>::value;
    break;
  case 0x1b:
    Vi = (((Vlp) * f.filterGain) >> 12) + ve + v2 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x1c:
    Vi = (((Vlp) * f.filterGain) >> 12) + ve + v3;
    offset = mixer_offset<3>::value;
    break;
  case 0x1d:
    Vi = (((Vlp) * f.filterGain) >> 12) + ve + v3 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x1e:
    Vi = (((Vlp) * f.filterGain) >> 12) + ve + v3 + v2;
    offset = mixer_offset<4>::value;
    break;
  case 0x1f:
    Vi = (((Vlp) * f.filterGain) >> 12) + ve + v3 + v2 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x20:
    Vi = (((Vbp) * f.filterGain) >> 12);
    offset = mixer_offset<1>::value;
    break;
  case 0x21:
    Vi = (((Vbp) * f.filterGain) >> 12) + v1;
    offset = mixer_offset<2>::value;
    break;
  case 0x22:
    Vi = (((Vbp) * f.filterGain) >> 12) + v2;
    offset = mixer_offset<2>::value;
    break;
  case 0x23:
    Vi = (((Vbp) * f.filterGain) >> 12) + v2 + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x24:
    Vi = (((Vbp) * f.filterGain) >> 12) + v3;
    offset = mixer_offset<2>::value;
    break;
  case 0x25:
    Vi = (((Vbp) * f.filterGain) >> 12) + v3 + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x26:
    Vi = (((Vbp) * f.filterGain) >> 12) + v3 + v2;
    offset = mixer_offset<3>::value;
    break;
  case 0x27:
    Vi = (((Vbp) * f.filterGain) >> 12) + v3 + v2 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x28:
    Vi = (((Vbp) * f.filterGain) >> 12) + ve;
    offset = mixer_offset<2>::value;
    break;
  case 0x29:
    Vi = (((Vbp) * f.filterGain) >> 12) + ve + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x2a:
    Vi = (((Vbp) * f.filterGain) >> 12) + ve + v2;
    offset = mixer_offset<3>::value;
    break;
  case 0x2b:
    Vi = (((Vbp) * f.filterGain) >> 12) + ve + v2 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x2c:
    Vi = (((Vbp) * f.filterGain) >> 12) + ve + v3;
    offset = mixer_offset<3>::value;
    break;
  case 0x2d:
    Vi = (((Vbp) * f.filterGain) >> 12) + ve + v3 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x2e:
    Vi = (((Vbp) * f.filterGain) >> 12) + ve + v3 + v2;
    offset = mixer_offset<4>::value;
    break;
  case 0x2f:
    Vi = (((Vbp) * f.filterGain) >> 12) + ve + v3 + v2 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x30:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12);
    offset = mixer_offset<2>::value;
    break;
  case 0x31:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x32:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + v2;
    offset = mixer_offset<3>::value;
    break;
  case 0x33:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + v2 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x34:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + v3;
    offset = mixer_offset<3>::value;
    break;
  case 0x35:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + v3 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x36:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + v3 + v2;
    offset = mixer_offset<4>::value;
    break;
  case 0x37:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + v3 + v2 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x38:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + ve;
    offset = mixer_offset<3>::value;
    break;
  case 0x39:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + ve + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x3a:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + ve + v2;
    offset = mixer_offset<4>::value;
    break;
  case 0x3b:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + ve + v2 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x3c:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + ve + v3;
    offset = mixer_offset<4>::value;
    break;
  case 0x3d:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + ve + v3 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x3e:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + ve + v3 + v2;
    offset = mixer_offset<5>::value;
    break;
  case 0x3f:
    Vi = (((Vbp + Vlp) * f.filterGain) >> 12) + ve + v3 + v2 + v1;
    offset = mixer_offset<6>::value;
    break;
  case 0x40:
    Vi = (((Vhp) * f.filterGain) >> 12);
    offset = mixer_offset<1>::value;
    break;
  case 0x41:
    Vi = (((Vhp) * f.filterGain) >> 12) + v1;
    offset = mixer_offset<2>::value;
    break;
  case 0x42:
    Vi = (((Vhp) * f.filterGain) >> 12) + v2;
    offset = mixer_offset<2>::value;
    break;
  case 0x43:
    Vi = (((Vhp) * f.filterGain) >> 12) + v2 + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x44:
    Vi = (((Vhp) * f.filterGain) >> 12) + v3;
    offset = mixer_offset<2>::value;
    break;
  case 0x45:
    Vi = (((Vhp) * f.filterGain) >> 12) + v3 + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x46:
    Vi = (((Vhp) * f.filterGain) >> 12) + v3 + v2;
    offset = mixer_offset<3>::value;
    break;
  case 0x47:
    Vi = (((Vhp) * f.filterGain) >> 12) + v3 + v2 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x48:
    Vi = (((Vhp) * f.filterGain) >> 12) + ve;
    offset = mixer_offset<2>::value;
    break;
  case 0x49:
    Vi = (((Vhp) * f.filterGain) >> 12) + ve + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x4a:
    Vi = (((Vhp) * f.filterGain) >> 12) + ve + v2;
    offset = mixer_offset<3>::value;
    break;
  case 0x4b:
    Vi = (((Vhp) * f.filterGain) >> 12) + ve + v2 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x4c:
    Vi = (((Vhp) * f.filterGain) >> 12) + ve + v3;
    offset = mixer_offset<3>::value;
    break;
  case 0x4d:
    Vi = (((Vhp) * f.filterGain) >> 12) + ve + v3 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x4e:
    Vi = (((Vhp) * f.filterGain) >> 12) + ve + v3 + v2;
    offset = mixer_offset<4>::value;
    break;
  case 0x4f:
    Vi = (((Vhp) * f.filterGain) >> 12) + ve + v3 + v2 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x50:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12);
    offset = mixer_offset<2>::value;
    break;
  case 0x51:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x52:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + v2;
    offset = mixer_offset<3>::value;
    break;
  case 0x53:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + v2 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x54:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + v3;
    offset = mixer_offset<3>::value;
    break;
  case 0x55:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + v3 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x56:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + v3 + v2;
    offset = mixer_offset<4>::value;
    break;
  case 0x57:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + v3 + v2 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x58:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + ve;
    offset = mixer_offset<3>::value;
    break;
  case 0x59:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + ve + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x5a:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + ve + v2;
    offset = mixer_offset<4>::value;
    break;
  case 0x5b:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + ve + v2 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x5c:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + ve + v3;
    offset = mixer_offset<4>::value;
    break;
  case 0x5d:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + ve + v3 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x5e:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + ve + v3 + v2;
    offset = mixer_offset<5>::value;
    break;
  case 0x5f:
    Vi = (((Vhp + Vlp) * f.filterGain) >> 12) + ve + v3 + v2 + v1;
    offset = mixer_offset<6>::value;
    break;
  case 0x60:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12);
    offset = mixer_offset<2>::value;
    break;
  case 0x61:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + v1;
    offset = mixer_offset<3>::value;
    break;
  case 0x62:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + v2;
    offset = mixer_offset<3>::value;
    break;
  case 0x63:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + v2 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x64:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + v3;
    offset = mixer_offset<3>::value;
    break;
  case 0x65:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + v3 + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x66:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + v3 + v2;
    offset = mixer_offset<4>::value;
    break;
  case 0x67:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + v3 + v2 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x68:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + ve;
    offset = mixer_offset<3>::value;
    break;
  case 0x69:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + ve + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x6a:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + ve + v2;
    offset = mixer_offset<4>::value;
    break;
  case 0x6b:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + ve + v2 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x6c:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + ve + v3;
    offset = mixer_offset<4>::value;
    break;
  case 0x6d:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + ve + v3 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x6e:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + ve + v3 + v2;
    offset = mixer_offset<5>::value;
    break;
  case 0x6f:
    Vi = (((Vhp + Vbp) * f.filterGain) >> 12) + ve + v3 + v2 + v1;
    offset = mixer_offset<6>::value;
    break;
  case 0x70:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12);
    offset = mixer_offset<3>::value;
    break;
  case 0x71:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + v1;
    offset = mixer_offset<4>::value;
    break;
  case 0x72:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + v2;
    offset = mixer_offset<4>::value;
    break;
  case 0x73:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + v2 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x74:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + v3;
    offset = mixer_offset<4>::value;
    break;
  case 0x75:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + v3 + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x76:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + v3 + v2;
    offset = mixer_offset<5>::value;
    break;
  case 0x77:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + v3 + v2 + v1;
    offset = mixer_offset<6>::value;
    break;
  case 0x78:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + ve;
    offset = mixer_offset<4>::value;
    break;
  case 0x79:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + ve + v1;
    offset = mixer_offset<5>::value;
    break;
  case 0x7a:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + ve + v2;
    offset = mixer_offset<5>::value;
    break;
  case 0x7b:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + ve + v2 + v1;
    offset = mixer_offset<6>::value;
    break;
  case 0x7c:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + ve + v3;
    offset = mixer_offset<5>::value;
    break;
  case 0x7d:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + ve + v3 + v1;
    offset = mixer_offset<6>::value;
    break;
  case 0x7e:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + ve + v3 + v2;
    offset = mixer_offset<6>::value;
    break;
  case 0x7f:
    Vi = (((Vhp + Vbp + Vlp) * f.filterGain) >> 12) + ve + v3 + v2 + v1;
    offset = mixer_offset<7>::value;
    break;
  }

  // Sum the inputs in the mixer and run the mixer output through the gain.
  const int idx1 = offset + Vi;
  assert((idx1 >= 0) && (idx1 < mixer_offset<8>::value));
  const int idx2 = f.mixer[idx1];
  assert((idx2 >= 0) && (idx2 < (1 << 16)));
  return (short)(f.gain[vol][idx2] - (1 << 15));
}


/*
Find output voltage in inverting gain and inverting summer SID op-amp
circuits, using a combination of Newton-Raphson and bisection.

             ---R2--
            |       |
  vi ---R1-----[A>----- vo
            vx

From Kirchoff's current law it follows that

  IR1f + IR2r = 0

Substituting the triode mode transistor model K*W/L*(Vgst^2 - Vgdt^2)
for the currents, we get:

  n*((Vddt - vx)^2 - (Vddt - vi)^2) + (Vddt - vx)^2 - (Vddt - vo)^2 = 0

Our root function f can thus be written as:

  f = (n + 1)*(Vddt - vx)^2 - n*(Vddt - vi)^2 - (Vddt - vo)^2 = 0

We are using the mapping function x = vo - vx -> vx. We thus substitute
for vo = vx + x and get:

  f(vx) = (n + 1)*(Vddt - vx)^2 - n*(Vddt - vi)^2 - (Vddt - (vx + x))^2 = 0

Using substitution constants

  a = n + 1
  b = Vddt
  c = n*(Vddt - vi)^2

the equations for the root function can be written and expanded as:

  f(vx) = a*(b - vx)^2 - c - (b - (vx + x))^2
        = a*(b^2 + vx^2 - 2*b*vx) - c - (b^2 + (vx + x)^2 - 2*b*(vx + x))
        = a*b^2 + a*vx^2 - 2*a*b*vx - c - b^2 - (vx + x)^2 + 2*b*(vx + x)
        = a*b^2 + a*vx^2 - 2*a*b*vx - c - b^2 - vx^2 - x^2 - 2*x*vx + 2*b*vx + 2*b*x

Then we calculate the derivative:

  f'(vx) = 2*a*vx - 2*a*b  - 2*vx - 2*x + 2*b
         = 2*(a*vx - a*b - vx - x + b)
         = 2*(a*(vx - b) + b - (vx + x))
         = 2*(b - (vx + x) - a*(b - vx))
         = 2*(b - vo - a*(b - vx))

Given f'(x) = df/dx, we have the resulting

  df = 2*((b - (vx + x)) - a*(b - vx))*dvx
*/
#if 0
RESID_INLINE
int Filter::solve_gain(opamp_t* opamp, int n, int vi, int& x, model_filter_t& mf)
{
  // Note that all variables are translated and scaled in order to fit
  // in 16 bits. It is not necessary to explicitly translate the variables here,
  // since they are all used in subtractions which cancel out the translation:
  // (a - t) - (b - t) = a - b

  // Start off with an estimate of x and a root bracket [ak, bk].
  // f is increasing, so that f(ak) < 0 and f(bk) > 0.
  int ak = mf.ak, bk = mf.bk;

  int a = n + (1 << 7);              // Scaled by 2^7
  int b = mf.kVddt;                  // Scaled by m*2^16
  int b_vi = b - vi;                 // Scaled by m*2^16
  if (b_vi < 0) b_vi = 0;
  int c = n*int(unsigned(b_vi)*unsigned(b_vi) >> 12);    // Scaled by m^2*2^27

  for (;;) {
    int xk = x;

    // Calculate f and df.
    int vx = opamp[x].vx;      // Scaled by m*2^16
    int dvx = opamp[x].dvx;    // Scaled by 2^11

    // f = a*(b - vx)^2 - c - (b - vo)^2
    // df = 2*((b - vo)*(dvx + 1) - a*(b - vx)*dvx)
    //
    int vo = vx + (x << 1) - (1 << 16);
    if (vo >= (1 << 16)) {
      vo = (1 << 16) - 1;
    }
    else if (vo < 0) {
      vo = 0;
    }
    int b_vx = b - vx;
    if (b_vx < 0) b_vx = 0;
    int b_vo = b - vo;
    if (b_vo < 0) b_vo = 0;
    // The dividend is scaled by m^2*2^27.
    int f = a*int(unsigned(b_vx)*unsigned(b_vx) >> 12) - c - int(unsigned(b_vo)*unsigned(b_vo) >> 5);
    // The divisor is scaled by m*2^11.
    int df = ((b_vo*(dvx + (1 << 11)) >> 1) - (a*(b_vx*dvx >> 8))) >> 14;
    // The resulting quotient is thus scaled by m*2^16.

    // Newton-Raphson step: xk1 = xk - f(xk)/f'(xk)
    // If f(xk) or f'(xk) are zero then we can't improve further.
    if (df) {
        x -= f/df;
    }
    if (unlikely(x == xk)) {
      // No further root improvement possible.
      return vo;
    }

    // Narrow down root bracket.
    if (f < 0) {
      // f(xk) < 0
      ak = xk;
    }
    else {
      // f(xk) > 0
      bk = xk;
    }

    if (unlikely(x <= ak) || unlikely(x >= bk)) {
      // Bisection step (ala Dekker's method).
      x = (ak + bk) >> 1;
      if (unlikely(x == ak)) {
        // No further bisection possible.
        return vo;
      }
    }
  }
}
#endif
RESID_INLINE
int Filter::solve_gain_d(opamp_t* opamp, double n, int vi, int& x, model_filter_t& mf)
{
  // Note that all variables are translated and scaled in order to fit
  // in 16 bits. It is not necessary to explicitly translate the variables here,
  // since they are all used in subtractions which cancel out the translation:
  // (a - t) - (b - t) = a - b

  // Start off with an estimate of x and a root bracket [ak, bk].
  // f is increasing, so that f(ak) < 0 and f(bk) > 0.
  int ak = mf.ak, bk = mf.bk;

  double a = n + 1.;
  int b = mf.kVddt;                            // Scaled by m*2^16
  double b_vi = b > vi ? double(b - vi) : 0.;  // Scaled by m*2^16
  double c = n*(b_vi*b_vi);                    // Scaled by m^2*2^32

  for (;;) {
    int xk = x;

    // Calculate f and df.
    int vx = opamp[x].vx;      // Scaled by m*2^16
    int dvx = opamp[x].dvx;    // Scaled by m*2^11

    // f = a*(b - vx)^2 - c - (b - vo)^2
    // df = 2*((b - vo) - a*(b - vx))*dvx
    //
    int vo = vx + (x << 1) - (1 << 16);
    if (vo > (1 << 16) - 1) {
      vo = (1 << 16) - 1;
    }
    else if (vo < 0) {
      vo = 0;
    }
    double b_vx = b > vx ? double(b - vx) : 0.;
    double b_vo = b > vo ? double(b - vo) : 0.;
    // The dividend is scaled by m^2*2^32.
    double f = a*(b_vx*b_vx) - c - (b_vo*b_vo);
    // The divisor is scaled by m*2^27.
    double df = 2.*(b_vo - a*b_vx)*double(dvx);
    // The resulting quotient is thus scaled by m*2^5.

    // Newton-Raphson step: xk1 = xk - f(xk)/f'(xk)
    // If f(xk) or f'(xk) are zero then we can't improve further.
    if (df) {
        // Multiply by 2^11 so it's scaled by m*2^16.
        x -= int(double(1<<11)*f/df);
    }
    if (unlikely(x == xk)) {
      // No further root improvement possible.
      return vo;
    }

    // Narrow down root bracket.
    if (f < 0) {
      // f(xk) < 0
      ak = xk;
    }
    else {
      // f(xk) > 0
      bk = xk;
    }

    if (unlikely(x <= ak) || unlikely(x >= bk)) {
      // Bisection step (ala Dekker's method).
      x = (ak + bk) >> 1;
      if (unlikely(x == ak)) {
        // No further bisection possible.
        return vo;
      }
    }
  }
}


/*
Find output voltage in inverting integrator SID op-amp circuits, using a
single fixpoint iteration step.

A circuit diagram of a MOS 6581 integrator is shown below.

                 ---C---
                |       |
  vi -----Rw-------[A>----- vo
       |      | vx
        --Rs--

From Kirchoff's current law it follows that

  IRw + IRs + ICr = 0

Using the formula for current through a capacitor, i = C*dv/dt, we get

  IRw + IRs + C*(vc - vc0)/dt = 0
  dt/C*(IRw + IRs) + vc - vc0 = 0
  vc = vc0 - n*(IRw(vi,vx) + IRs(vi,vx))

which may be rewritten as the following iterative fixpoint function:

  vc = vc0 - n*(IRw(vi,g(vc)) + IRs(vi,g(vc)))

To accurately calculate the currents through Rs and Rw, we need to use
transistor models. Rs has a gate voltage of Vdd = 12V, and can be
assumed to always be in triode mode. For Rw, the situation is rather
more complex, as it turns out that this transistor will operate in
both subthreshold, triode, and saturation modes.

The Shichman-Hodges transistor model routinely used in textbooks may
be written as follows:

  Ids = 0                          , Vgst < 0               (subthreshold mode)
  Ids = K/2*W/L*(2*Vgst - Vds)*Vds , Vgst >= 0, Vds < Vgst  (triode mode)
  Ids = K/2*W/L*Vgst^2             , Vgst >= 0, Vds >= Vgst (saturation mode)

  where
  K   = u*Cox (conductance)
  W/L = ratio between substrate width and length
  Vgst = Vg - Vs - Vt (overdrive voltage)

This transistor model is also called the quadratic model.

Note that the equation for the triode mode can be reformulated as
independent terms depending on Vgs and Vgd, respectively, by the
following substitution:

  Vds = Vgst - (Vgst - Vds) = Vgst - Vgdt

  Ids = K/2*W/L*(2*Vgst - Vds)*Vds
  = K/2*W/L*(2*Vgst - (Vgst - Vgdt)*(Vgst - Vgdt)
  = K/2*W/L*(Vgst + Vgdt)*(Vgst - Vgdt)
  = K/2*W/L*(Vgst^2 - Vgdt^2)

This turns out to be a general equation which covers both the triode
and saturation modes (where the second term is 0 in saturation mode).
The equation is also symmetrical, i.e. it can calculate negative
currents without any change of parameters (since the terms for drain
and source are identical except for the sign).

FIXME: Subthreshold as function of Vgs, Vgd.
  Ids = I0*e^(Vgst/(n*VT))       , Vgst < 0               (subthreshold mode)

The remaining problem with the textbook model is that the transition
from subthreshold the triode/saturation is not continuous.

Realizing that the subthreshold and triode/saturation modes may both
be defined by independent (and equal) terms of Vgs and Vds,
respectively, the corresponding terms can be blended into (equal)
continuous functions suitable for table lookup.

The EKV model (Enz, Krummenacher and Vittoz) essentially performs this
blending using an elegant mathematical formulation:

  Ids = Is*(if - ir)
  Is = ((2*u*Cox*Ut^2)/k)*W/L
  if = ln^2(1 + e^((k*(Vg - Vt) - Vs)/(2*Ut))
  ir = ln^2(1 + e^((k*(Vg - Vt) - Vd)/(2*Ut))

For our purposes, the EKV model preserves two important properties
discussed above:

- It consists of two independent terms, which can be represented by
  the same lookup table.
- It is symmetrical, i.e. it calculates current in both directions,
  facilitating a branch-free implementation.

Rw in the circuit diagram above is a VCR (voltage controlled resistor),
as shown in the circuit diagram below.

                   Vw

                   |
           Vdd     |
              |---|
             _|_   |
           --    --| Vg
          |      __|__
          |      -----  Rw
          |      |   |
  vi ------------     -------- vo


In order to calculalate the current through the VCR, its gate voltage
must be determined.

Assuming triode mode and applying Kirchoff's current law, we get the
following equation for Vg:

u*Cox/2*W/L*((Vddt - Vg)^2 - (Vddt - vi)^2 + (Vddt - Vg)^2 - (Vddt - Vw)^2) = 0
2*(Vddt - Vg)^2 - (Vddt - vi)^2 - (Vddt - Vw)^2 = 0
(Vddt - Vg) = sqrt(((Vddt - vi)^2 + (Vddt - Vw)^2)/2)

Vg = Vddt - sqrt(((Vddt - vi)^2 + (Vddt - Vw)^2)/2)

*/
RESID_INLINE
int Filter::solve_integrate_6581(int dt, int vi, int& vx, int& vc, model_filter_t& mf)
{
  // Note that all variables are translated and scaled in order to fit
  // in 16 bits. It is not necessary to explicitly translate the variables here,
  // since they are all used in subtractions which cancel out the translation:
  // (a - t) - (b - t) = a - b

  int kVddt = mf.kVddt;      // Scaled by m*2^16

  // "Snake" voltages for triode mode calculation.
  unsigned int Vgst = kVddt - vx;
  unsigned int Vgdt = kVddt - vi;
  unsigned int Vgdt_2 = Vgdt*Vgdt;

  // "Snake" current, scaled by (1/m)*2^13*m*2^16*m*2^16*2^-15 = m*2^30
  int n_I_snake = n_snake*(int(Vgst*Vgst - Vgdt_2) >> 15);

  // VCR gate voltage.       // Scaled by m*2^16
  // Vg = Vddt - sqrt(((Vddt - Vw)^2 + Vgdt^2)/2)
  int kVg = vcr_kVg[(Vddt_Vw_2 + (Vgdt_2 >> 1)) >> 16];

  // VCR voltages for EKV model table lookup.
  int Vgs = kVg - vx + (1 << 15);
  int Vgd = kVg - vi + (1 << 15);

  // VCR current, scaled by m*2^15*2^15 = m*2^30
  int n_I_vcr = int(unsigned(vcr_n_Ids_term[Vgs] - vcr_n_Ids_term[Vgd]) << 15);

  // Change in capacitor charge.
  vc -= (n_I_snake + n_I_vcr)*dt;

/*
  // FIXME: Determine whether this check is necessary.
  if (vc < mf.vc_min) {
    vc = mf.vc_min;
  }
  else if (vc > mf.vc_max) {
    vc = mf.vc_max;
  }
*/

  // vx = g(vc)
  const int idx = (vc >> 15) + (1 << 15);
  assert((idx >= 0) && (idx < (1 << 16)));
  vx = mf.opamp_rev[idx];

  // Return vo.
  return vx + (vc >> 14);
}

/*
The 8580 integrator is similar to those found in 6581
but the resistance is formed by multiple NMOS transistors
in parallel controlled by the fc bits where the gate voltage
is driven by a temperature dependent voltage divider.

                 ---C---
                |       |
  vi -----Rfc------[A>----- vo
                vx

  IRfc + ICr = 0
  IRfc + C*(vc - vc0)/dt = 0
  dt/C*(IRfc) + vc - vc0 = 0
  vc = vc0 - n*(IRfc(vi,vx))
  vc = vc0 - n*(IRfc(vi,g(vc)))

IRfc = K/2*W/L*(Vgst^2 - Vgdt^2) = n*((Vgt - vx)^2 - (Vgt - vi)^2)
*/
RESID_INLINE
int Filter::solve_integrate_8580(int dt, int vi, int& vx, int& vc, model_filter_t& mf)
{
  // Note that all variables are translated and scaled in order to fit
  // in 16 bits. It is not necessary to explicitly translate the variables here,
  // since they are all used in subtractions which cancel out the translation:
  // (a - t) - (b - t) = a - b

  // Dac voltages.
  unsigned int Vgst = nVgt - vx;
  unsigned int Vgdt = (vi < nVgt) ? nVgt - vi : 0;  // triode/saturation mode

  // Dac current, scaled by (1/m)*2^13*m*2^16*m*2^16*2^-15 = m*2^30
  int n_I_rfc = (n_dac*(int(Vgst*Vgst - Vgdt*Vgdt) >> 15)) >> 4;

  // Change in capacitor charge.
  vc -= n_I_rfc*dt;

  // vx = g(vc)
  const int idx = (vc >> 15) + (1 << 15);
  assert((idx >= 0) && (idx < (1 << 16)));
  vx = mf.opamp_rev[idx];

  // Return vo.
  return vx + (vc >> 14);
}

#endif // RESID_INLINING || defined(RESID_FILTER_CC)

} // namespace reSID

#endif // not RESID_FILTER_H
