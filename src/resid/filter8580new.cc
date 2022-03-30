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

#define RESID_FILTER_CC

#ifdef _M_ARM
#undef _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE
#define _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE 1
#endif

#include "filter8580new.h"
#include "dac.h"
#include "spline.h"
#include <math.h>

namespace reSID
{

// This is the SID 6581 op-amp voltage transfer function, measured on
// CAP1B/CAP1A on a chip marked MOS 6581R4AR 0687 14.
// All measured chips have op-amps with output voltages (and thus input
// voltages) within the range of 0.81V - 10.31V.

static double_point opamp_voltage_6581[] = {
  {  0.81, 10.31 },  // Approximate start of actual range
  {  0.81, 10.31 },  // Repeated point
  {  2.40, 10.31 },
  {  2.60, 10.30 },
  {  2.70, 10.29 },
  {  2.80, 10.26 },
  {  2.90, 10.17 },
  {  3.00, 10.04 },
  {  3.10,  9.83 },
  {  3.20,  9.58 },
  {  3.30,  9.32 },
  {  3.50,  8.69 },
  {  3.70,  8.00 },
  {  4.00,  6.89 },
  {  4.40,  5.21 },
  {  4.54,  4.54 },  // Working point (vi = vo)
  {  4.60,  4.19 },
  {  4.80,  3.00 },
  {  4.90,  2.30 },  // Change of curvature
  {  4.95,  2.03 },
  {  5.00,  1.88 },
  {  5.05,  1.77 },
  {  5.10,  1.69 },
  {  5.20,  1.58 },
  {  5.40,  1.44 },
  {  5.60,  1.33 },
  {  5.80,  1.26 },
  {  6.00,  1.21 },
  {  6.40,  1.12 },
  {  7.00,  1.02 },
  {  7.50,  0.97 },
  {  8.50,  0.89 },
  { 10.00,  0.81 },
  { 10.31,  0.81 },  // Approximate end of actual range
  { 10.31,  0.81 }   // Repeated end point
};

// This is the SID 8580 op-amp voltage transfer function, measured on
// CAP1B/CAP1A on a chip marked CSG 8580R5 1690 25.
static double_point opamp_voltage_8580[] = {
  {  1.30,  8.91 },  // Approximate start of actual range
  {  1.30,  8.91 },  // Repeated end point
  {  4.76,  8.91 },
  {  4.77,  8.90 },
  {  4.78,  8.88 },
  {  4.785, 8.86 },
  {  4.79,  8.80 },
  {  4.795, 8.60 },
  {  4.80,  8.25 },
  {  4.805, 7.50 },
  {  4.81,  6.10 },
  {  4.815, 4.05 },  // Change of curvature
  {  4.82,  2.27 },
  {  4.825, 1.65 },
  {  4.83,  1.55 },
  {  4.84,  1.47 },
  {  4.85,  1.43 },
  {  4.87,  1.37 },
  {  4.90,  1.34 },
  {  5.00,  1.30 },
  {  5.10,  1.30 },
  {  8.91,  1.30 },  // Approximate end of actual range
  {  8.91,  1.30 }   // Repeated end point
};


/*
 * R1 = 15.3*Ri
 * R2 =  7.3*Ri
 * R3 =  4.7*Ri
 * Rf =  1.4*Ri
 * R4 =  1.4*Ri
 * R8 =  2.0*Ri
 * RC =  2.8*Ri
 *
 * res  feedback  input
 * ---  --------  -----
 *  0   Rf        Ri
 *  1   Rf|R1     Ri
 *  2   Rf|R2     Ri
 *  3   Rf|R3     Ri
 *  4   Rf        R4
 *  5   Rf|R1     R4
 *  6   Rf|R2     R4
 *  7   Rf|R3     R4
 *  8   Rf        R8
 *  9   Rf|R1     R8
 *  A   Rf|R2     R8
 *  B   Rf|R3     R8
 *  C   Rf        RC
 *  D   Rf|R1     RC
 *  E   Rf|R2     RC
 *  F   Rf|R3     RC
 */
static int resGain[16] =
{
  (int)((1<<7)*(1.4/1.0)),                     //      Rf/Ri   1.4
  (int)((1<<7)*(((1.4*15.3)/(1.4+15.3))/1.0)), // (Rf|R1)/Ri   1.28263
  (int)((1<<7)*(((1.4*7.3)/(1.4+7.3))/1.0)),   // (Rf|R2)/Ri   1.17471
  (int)((1<<7)*(((1.4*4.7)/(1.4+4.7))/1.0)),   // (Rf|R3)/Ri   1.07869
  (int)((1<<7)*(1.4/1.4)),                     //      Rf/R4   1.0
  (int)((1<<7)*(((1.4*15.3)/(1.4+15.3))/1.4)), // (Rf|R1)/R4   0.916168
  (int)((1<<7)*(((1.4*7.3)/(1.4+7.3))/1.4)),   // (Rf|R2)/R4   0.83908
  (int)((1<<7)*(((1.4*4.7)/(1.4+4.7))/1.4)),   // (Rf|R3)/R4   0.770492
  (int)((1<<7)*(1.4/2.0)),                     //      Rf/R8   0.7
  (int)((1<<7)*(((1.4*15.3)/(1.4+15.3))/2.0)), // (Rf|R1)/R8   0.641317
  (int)((1<<7)*(((1.4*7.3)/(1.4+7.3))/2.0)),   // (Rf|R2)/R8   0.587356
  (int)((1<<7)*(((1.4*4.7)/(1.4+4.7))/2.0)),   // (Rf|R3)/R8   0.539344
  (int)((1<<7)*(1.4/2.8)),                     //      Rf/RC   0.5
  (int)((1<<7)*(((1.4*15.3)/(1.4+15.3))/2.8)), // (Rf|R1)/RC   0.458084
  (int)((1<<7)*(((1.4*7.3)/(1.4+7.3))/2.8)),   // (Rf|R2)/RC   0.41954
  (int)((1<<7)*(((1.4*4.7)/(1.4+4.7))/2.8)),   // (Rf|R3)/RC   0.385246
};

typedef struct {
  // Op-amp transfer function.
  double_point* opamp_voltage;
  int opamp_voltage_size;
  // Voice output characteristics.
  double voice_voltage_range;
  double voice_DC_voltage;
  // Capacitor value.
  double C;
  // Transistor parameters.
  double Vdd;
  double Vth;        // Threshold voltage
  double Ut;         // Thermal voltage: Ut = k*T/q = 8.61734315e-5*T ~ 26mV
  double k;          // Gate coupling coefficient: K = Cox/(Cox+Cdep) ~ 0.7
  double uCox;       // u*Cox
  double WL_vcr;     // W/L for VCR
  double WL_snake;   // W/L for "snake"
  // DAC parameters.
  double dac_zero;
  double dac_scale;
  double dac_2R_div_R;
  bool dac_term;
} model_filter_init_t;

static model_filter_init_t model_filter_init[2] = {
  {
    opamp_voltage_6581,
    sizeof(opamp_voltage_6581)/sizeof(*opamp_voltage_6581),
    // The dynamic analog range of one voice is approximately 1.5V,
    1.5,
    // riding at a DC level of approximately 5.0V.
    5.075,
    // Capacitor value.
    470e-12,
    // Transistor parameters.
    12.18,
    1.31,
    26.0e-3,
    1.0,
    20e-6,
    9.0/1.0,
    1.0/115,
    // DAC parameters.
    6.65,
    2.63,
    2.20,
    false
  },
  {
    opamp_voltage_8580,
    sizeof(opamp_voltage_8580)/sizeof(*opamp_voltage_8580),
    // FIXME: Measure for the 8580.
    0.25,
    // The 4.75V voltage for the virtual ground is generated by a PolySi resistor divider
    4.80, // FIXME
    // Capacitor value.
    22e-9,
    // Transistor parameters.
    9.09,
    0.80,
    26.0e-3,
    1.0,   // Unused, leave at 1
    100e-6,
    // FIXME: 6581 only
    0,
    0,
    0,
    0,
    2.00,
    true
  }
};

unsigned short Filter::vcr_kVg[1 << 16];
unsigned short Filter::vcr_n_Ids_term[1 << 16];
int Filter::n_snake;
int Filter::n_param;

#if defined(__amiga__) && defined(__mc68000__)
#undef HAS_LOG1P
#endif

#ifndef HAS_LOG1P
static double log1p(double x)
{
    return log(1 + x) - (((1 + x) - 1) - x) / (1 + x);
}
#endif

Filter::model_filter_t Filter::model_filter[2];


// ----------------------------------------------------------------------------
// Constructor.
// ----------------------------------------------------------------------------
Filter::Filter()
{
  static bool class_init;

  if (!class_init) {
    double tmp_n_param[2];

    unsigned int dac_bits = 11;

    // Temporary tables for op-amp transfer function.
    unsigned int* voltages = new unsigned int[1 << 16];
    opamp_t* opamp = new opamp_t[1 << 16];

    for (int m = 0; m < 2; m++) {
      model_filter_init_t& fi = model_filter_init[m];
      model_filter_t& mf = model_filter[m];

      // Convert op-amp voltage transfer to 16 bit values.
      double vmin = fi.opamp_voltage[0][0];
      double opamp_max = fi.opamp_voltage[0][1];
      double kVddt = fi.k*(fi.Vdd - fi.Vth);
      double vmax = kVddt < opamp_max ? opamp_max : kVddt;
      double denorm = vmax - vmin;
      double norm = 1.0/denorm;

      // Scaling and translation constants.
      double N16 = norm*((1u << 16) - 1);
      double N30 = norm*((1u << 30) - 1);
      double N31 = norm*((1u << 31) - 1);
      mf.vo_N16 = N16;

      // The "zero" output level of the voices.
      // The digital range of one voice is 20 bits; create a scaling term
      // for multiplication which fits in 11 bits.
      double N14 = norm*(1u << 14);
      mf.voice_scale_s14 = (int)(N14*fi.voice_voltage_range);
      mf.voice_DC = (int)(N16*(fi.voice_DC_voltage - vmin));

      // Vdd - Vth, normalized so that translated values can be subtracted:
      // k*Vddt - x = (k*Vddt - t) - (x - t)
      mf.kVddt = (int)(N16*(kVddt - vmin) + 0.5);

      tmp_n_param[m] = denorm*(1 << 13)*((fi.uCox/2.)*1.0e-6/fi.C);

      // Create lookup table mapping op-amp voltage across output and input
      // to input voltage: vo - vx -> vx
      // FIXME: No variable length arrays in ISO C++, hardcoding to max 50
      // points.
      // double_point scaled_voltage[fi.opamp_voltage_size];
      double_point scaled_voltage[50];

      for (int i = 0; i < fi.opamp_voltage_size; i++) {
        // The target output range is 16 bits, in order to fit in an unsigned
        // short.
        //
        // The y axis is temporarily scaled to 31 bits for maximum accuracy in
        // the calculated derivative.
        //
        // Values are normalized using
        //
        //   x_n = m*2^N*(x - xmin)
        //
        // and are translated back later (for fixed point math) using
        //
        //   m*2^N*x = x_n - m*2^N*xmin
        //
        scaled_voltage[fi.opamp_voltage_size - 1 - i][0] = N16*(fi.opamp_voltage[i][1] - fi.opamp_voltage[i][0] + denorm)/2.;
        scaled_voltage[fi.opamp_voltage_size - 1 - i][1] = N31*(fi.opamp_voltage[i][0] - vmin);
      }

      // Clamp x to 16 bit range (rounding may cause overflow).
      if (scaled_voltage[fi.opamp_voltage_size - 1][0] > 65535.) {
        // The last point is repeated.
        scaled_voltage[fi.opamp_voltage_size - 1][0] =
            scaled_voltage[fi.opamp_voltage_size - 2][0] = 65535.;
      }

      interpolate(scaled_voltage, scaled_voltage + fi.opamp_voltage_size - 1,
                    PointPlotter<unsigned int>(voltages), 1.0);

      // Store both fn and dfn in the same table.
      mf.ak = (int)(scaled_voltage[0][0] + 0.5);
      mf.bk = (int)(scaled_voltage[fi.opamp_voltage_size - 1][0] + 0.5);
      int j;
      for (j = 0; j < mf.ak; j++) {
        opamp[j].vx = 0;
        opamp[j].dvx = 0;
      }
      unsigned int f = voltages[j];
      for (; j <= mf.bk; j++) {
        unsigned int fp = f;
        f = voltages[j];  // Scaled by m*2^31
        // m*2^31*dy/1 = (m*2^31*dy)/(m*2^16*dx) = 2^15*dy/dx
        int df = f - fp;  // Scaled by 2^15

        // 16 bits unsigned: m*2^16*(fn - xmin)
        opamp[j].vx = f > (0xffff << 15) ? 0xffff : f >> 15;
        // 16 bits (15 bits + sign bit): 2^11*dfn
        opamp[j].dvx = df >> (15 - 11);
      }
      for (; j < (1 << 16); j++) {
        opamp[j].vx = 0;
        opamp[j].dvx = 0;
      }

      // We don't have the differential for the first point so just assume
      // it's the same as the second point's
      opamp[mf.ak].dvx = opamp[mf.ak+1].dvx;

      // Create lookup tables.

      // The filter summer operates at n ~ 1, and has 5 fundamentally different
      // input configurations (2 - 6 input "resistors").
      //
      // Note that all "on" transistors are modeled as one. This is not
      // entirely accurate, since the input for each transistor is different,
      // and transistors are not linear components. However modeling all
      // transistors separately would be extremely costly.
      int offset = 0;
      int size;
      for (int k = 0; k < 5; k++) {
        int idiv = 2 + k;        // 2 - 6 input "resistors".
        int n_idiv = idiv << 7;  // n*idiv, scaled by 2^7
        size = idiv << 16;
        int x = mf.ak;
        for (int vi = 0; vi < size; vi++) {
          mf.summer[offset + vi] =
            solve_gain(opamp, n_idiv, vi/idiv, x, mf);
        }
        offset += size;
      }

      // The audio mixer operates at n ~ 8/6 (6581) 8/5 (8580),
      // and has 8 fundamentally different
      // input configurations (0 - 7 input "resistors").
      //
      // All "on", transistors are modeled as one - see comments above for
      // the filter summer.
      int divider = m==0 ? 6 : 5;
      offset = 0;
      size = 1;  // Only one lookup element for 0 input "resistors".
      for (int l = 0; l < 8; l++) {
        int idiv = l;                 // 0 - 7 input "resistors".
        int n_idiv = (idiv << 7)*8/divider; // n*idiv, scaled by 2^7
        if (idiv == 0) {
          // Avoid division by zero; the result will be correct since
          // n_idiv = 0.
          idiv = 1;
        }
        int x = mf.ak;
        for (int vi = 0; vi < size; vi++) {
          mf.mixer[offset + vi] =
            solve_gain(opamp, n_idiv, vi/idiv, x, mf);
        }
        offset += size;
        size = (l + 1) << 16;
      }

      // 4 bit "resistor" ladders in the audio
      // output gain necessitate 16 gain tables.
      // From die photographs of the volume "resistor" ladders
      // it follows that gain ~ vol/12 (6581) vol/16 (8580)
      // (assuming ideal op-amps and ideal "resistors").
      divider = m==0 ? 12 : 16;
      for (int n8 = 0; n8 < 16; n8++) {
        int n = (n8 << 7) / divider;  // Scaled by 2^7
        int x = mf.ak;
        for (int vi = 0; vi < (1 << 16); vi++) {
          mf.gain[n8][vi] = solve_gain(opamp, n, vi, x, mf);
        }
      }

      // Create lookup table mapping capacitor voltage to op-amp input voltage:
      // vc -> vx
      for (int i = 0; i < (1 << 16); i++) {
        mf.opamp_rev[i] = opamp[i].vx;
      }

      mf.vc_max = (int)(N30*(fi.opamp_voltage[0][1] - fi.opamp_voltage[0][0]));
      mf.vc_min = (int)(N30*(fi.opamp_voltage[fi.opamp_voltage_size - 1][1] - fi.opamp_voltage[fi.opamp_voltage_size - 1][0]));

      if (m == 0) {
        // 6581 only

        // In the MOS 6581, 1/Q is controlled linearly by res. From die photographs
        // of the resonance "resistor" ladder it follows that 1/Q ~ ~res/8
        // (assuming an ideal op-amp and ideal "resistors"). This implies that Q
        // ranges from 0.533 (res = 0) to 8 (res = E). For res = F, Q is actually
        // theoretically unlimited, which is quite unheard of in a filter
        // circuit.
        //
        // To obtain Q ~ 1/sqrt(2) = 0.707 for maximally flat frequency response,
        // res should be set to 4: Q = 8/~4 = 8/11 = 0.7272 (again assuming an ideal
        // op-amp and ideal "resistors").
        //
        // Q as low as 0.707 is not achievable because of low gain op-amps; res = 0
        // should yield the flattest possible frequency response at Q ~ 0.8 - 1.0
        // in the op-amp's pseudo-linear range (high amplitude signals will be
        // clipped). As resonance is increased, the filter must be clocked more
        // often to keep it stable.
        for (int n8 = 0; n8 < 16; n8++) {
          int n = (~n8 & 0xf) << (7 - 3);  // Scaled by 2^7
          int x = mf.ak;
          for (int vi = 0; vi < (1 << 16); vi++) {
            mf.resonance[n8][vi] = solve_gain(opamp, n, vi, x, mf);
          }
        }

        Vw_bias = 0;

        // Normalized snake current factor, 1 cycle at 1MHz.
        // Fit in 5 bits.
        n_snake = (int)(fi.WL_snake * tmp_n_param[0] + 0.5);

        // DAC table.
        build_dac_table(mf.f0_dac, dac_bits, fi.dac_2R_div_R, fi.dac_term);
        for (int n = 0; n < (1 << dac_bits); n++) {
          mf.f0_dac[n] = (unsigned short)(N16*(fi.dac_zero + mf.f0_dac[n]*fi.dac_scale/(1 << dac_bits) - vmin) + 0.5);
        }

        // VCR table.
        double k = fi.k;
        double kVddt = N16*(k*(fi.Vdd - fi.Vth));
        vmin *= N16;

        for (int i = 0; i < (1 << 16); i++) {
          // The table index is right-shifted 16 times in order to fit in
          // 16 bits; the argument to sqrt is thus multiplied by (1 << 16).
          //
          // The returned value must be corrected for translation. Vg always
          // takes part in a subtraction as follows:
          //
          //   k*Vg - Vx = (k*Vg - t) - (Vx - t)
          //
          // I.e. k*Vg - t must be returned.
          double Vg = kVddt - sqrt((double)i*(1 << 16));
          vcr_kVg[i] = (unsigned short)(k*Vg - vmin + 0.5);
        }

        /*
          EKV model:

          Ids = Is*(if - ir)
          Is = ((2*u*Cox*Ut^2)/k)*W/L
          if = ln^2(1 + e^((k*(Vg - Vt) - Vs)/(2*Ut))
          ir = ln^2(1 + e^((k*(Vg - Vt) - Vd)/(2*Ut))
        */
        double kVt = fi.k*fi.Vth;
        double Ut = fi.Ut;
        double Is = ((2*fi.uCox*Ut*Ut)/fi.k)*fi.WL_vcr;
        // Normalized current factor for 1 cycle at 1MHz.
        double N15 = N16/2;
        double n_Is = N15*1.0e-6/fi.C*Is;

        // kVg_Vx = k*Vg - Vx
        // I.e. if k != 1.0, Vg must be scaled accordingly.
        for (int kVg_Vx = 0; kVg_Vx < (1 << 16); kVg_Vx++) {
          double log_term = log1p(exp((kVg_Vx/N16 - kVt)/(2*Ut)));
          // Scaled by m*2^15
          vcr_n_Ids_term[kVg_Vx] = (unsigned short)(n_Is*log_term*log_term);
        }
      } else {
        // 8580 only

        // In the MOS 8580, the resonance "resistor" ladder above the bp feedback
        // op-amp is split in two parts; one ladder for the op-amp input and one
        // ladder for the op-amp feedback.
        //
        // input:         feedback:
        //
        //             Rf
        // Ri R4 RC R8    R3
        //             R2
        //             R1
        //
        //
        // The "resistors" are switched in as follows by bits in register $17:
        //
        // feedback:
        // R1: bit4&!bit5
        // R2: !bit4&bit5
        // R3: bit4&bit5
        // Rf: always on
        //
        // input:
        // R4: bit6&!bit7
        // R8: !bit6&bit7
        // RC: bit6&bit7
        // Ri: !(R4|R8|RC) = !(bit6|bit7) = !bit6&!bit7
        //
        //
        // The relative "resistor" values are approximately (using channel length):
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
        // Approximate values for 1/Q can now be found as follows (assuming an
        // ideal op-amp):
        //
        // res  feedback  input  -gain (1/Q)
        // ---  --------  -----  ----------
        // 0   Rf        Ri     Rf/Ri      = 1/(Ri*(1/Rf))      = 1/0.71
        // 1   Rf|R1     Ri     (Rf|R1)/Ri = 1/(Ri*(1/Rf+1/R1)) = 1/0.78
        // 2   Rf|R2     Ri     (Rf|R2)/Ri = 1/(Ri*(1/Rf+1/R2)) = 1/0.85
        // 3   Rf|R3     Ri     (Rf|R3)/Ri = 1/(Ri*(1/Rf+1/R3)) = 1/0.92
        // 4   Rf        R4     Rf/R4      = 1/(R4*(1/Rf))      = 1/1.00
        // 5   Rf|R1     R4     (Rf|R1)/R4 = 1/(R4*(1/Rf+1/R1)) = 1/1.10
        // 6   Rf|R2     R4     (Rf|R2)/R4 = 1/(R4*(1/Rf+1/R2)) = 1/1.20
        // 7   Rf|R3     R4     (Rf|R3)/R4 = 1/(R4*(1/Rf+1/R3)) = 1/1.30
        // 8   Rf        R8     Rf/R8      = 1/(R8*(1/Rf))      = 1/1.43
        // 9   Rf|R1     R8     (Rf|R1)/R8 = 1/(R8*(1/Rf+1/R1)) = 1/1.56
        // A   Rf|R2     R8     (Rf|R2)/R8 = 1/(R8*(1/Rf+1/R2)) = 1/1.70
        // B   Rf|R3     R8     (Rf|R3)/R8 = 1/(R8*(1/Rf+1/R3)) = 1/1.86
        // C   Rf        RC     Rf/RC      = 1/(RC*(1/Rf))      = 1/2.00
        // D   Rf|R1     RC     (Rf|R1)/RC = 1/(RC*(1/Rf+1/R1)) = 1/2.18
        // E   Rf|R2     RC     (Rf|R2)/RC = 1/(RC*(1/Rf+1/R2)) = 1/2.38
        // F   Rf|R3     RC     (Rf|R3)/RC = 1/(RC*(1/Rf+1/R3)) = 1/2.60
        //
        //
        // These data indicate that the following function for 1/Q has been
        // modeled in the MOS 8580:
        //
        // 1/Q = 2^(1/2)*2^(-x/8) = 2^(1/2 - x/8) = 2^((4 - x)/8)
        for (int n8 = 0; n8 < 16; n8++) {
          int x = mf.ak;
          for (int vi = 0; vi < (1 << 16); vi++) {
            mf.resonance[n8][vi] = solve_gain(opamp, resGain[n8], vi, x, mf);
          }
        }

        // scaled 5 bits
        n_param = (int)(tmp_n_param[1] * 32 + 0.5);

        double Vgt = (fi.voice_DC_voltage * 1.6) - fi.Vth;
        nVgt = (int)(N16 * (Vgt - vmin) + 0.5);

        // DAC table.
        // W/L ratio for frequency DAC, bits are proportional.
        // scaled 5 bits
        unsigned int dacWL = 806; // 0,00307464599609375 * 1024 * 256 (actual value is ~= 0.003075)
        mf.f0_dac[0] = dacWL >> 8;
        for (int n = 1; n < (1 << dac_bits); n++) {
          // Calculate W/L ratio for parallel NMOS resistances
          unsigned int wl = 0;
          for (unsigned int i = 0; i < dac_bits; i++) {
            unsigned int bitmask = 1 << i;
            if (n & bitmask) {
                wl += dacWL * (bitmask<<1);
            }
          }
          mf.f0_dac[n] = wl >> 8;
        }
      }
    }

    // Free temporary tables.
    delete[] voltages;
    delete[] opamp;

    class_init = true;
  }

  enable_filter(true);
  set_chip_model(MOS6581);
  set_voice_mask(0x07);
  input(0);
  reset();
}


// ----------------------------------------------------------------------------
// Enable filter.
// ----------------------------------------------------------------------------
void Filter::enable_filter(bool enable)
{
  enabled = enable;
  set_sum_mix();
}


// ----------------------------------------------------------------------------
// Adjust the DAC bias parameter of the filter.
// This gives user variable control of the exact CF -> center frequency
// mapping used by the filter.
// ----------------------------------------------------------------------------
void Filter::adjust_filter_bias(double dac_bias)
{
  Vw_bias = int(dac_bias*model_filter[0].vo_N16);
  set_w0();

  // Gate voltage is controlled by the switched capacitor voltage divider
  // Ua = Ue * v = 4.75v  1<v<2
  model_filter_init_t& fi = model_filter_init[1];
  double Vg = fi.voice_DC_voltage * (dac_bias*6./100. + 1.6);
  double Vgt = Vg - fi.Vth;
  double vmin = fi.opamp_voltage[0][0];

  // Vg - Vth, normalized so that translated values can be subtracted:
  // Vgt - x = (Vgt - t) - (x - t)
  nVgt = (int)(model_filter[1].vo_N16 * (Vgt - vmin) + 0.5);
}

// ----------------------------------------------------------------------------
// Set chip model.
// ----------------------------------------------------------------------------
void Filter::set_chip_model(chip_model model)
{
  sid_model = model;
  /* We initialize the state variables again just to make sure that
   * the earlier model didn't leave behind some foreign, unrecoverable
   * state. Hopefully set_chip_model() only occurs simultaneously with
   * reset(). */
  Vhp = 0;
  Vbp = Vbp_x = Vbp_vc = 0;
  Vlp = Vlp_x = Vlp_vc = 0;
}


// ----------------------------------------------------------------------------
// Mask for voices routed into the filter / audio output stage.
// Used to physically connect/disconnect EXT IN, and for test purposes
// (voice muting).
// ----------------------------------------------------------------------------
void Filter::set_voice_mask(reg4 mask)
{
  voice_mask = 0xf0 | (mask & 0x0f);
  set_sum_mix();
}


// ----------------------------------------------------------------------------
// SID reset.
// ----------------------------------------------------------------------------
void Filter::reset()
{
  fc = 0;
  res = 0;
  filt = 0;
  mode = 0;
  vol = 0;

  Vhp = 0;
  Vbp = Vbp_x = Vbp_vc = 0;
  Vlp = Vlp_x = Vlp_vc = 0;

  set_w0();
  set_sum_mix();
}


// ----------------------------------------------------------------------------
// Register functions.
// ----------------------------------------------------------------------------
void Filter::writeFC_LO(reg8 fc_lo)
{
  fc = (fc & 0x7f8) | (fc_lo & 0x007);
  set_w0();
}

void Filter::writeFC_HI(reg8 fc_hi)
{
  fc = ((fc_hi << 3) & 0x7f8) | (fc & 0x007);
  set_w0();
}

void Filter::writeRES_FILT(reg8 res_filt)
{
  res = (res_filt >> 4) & 0x0f;

  filt = res_filt & 0x0f;
  set_sum_mix();
}

void Filter::writeMODE_VOL(reg8 mode_vol)
{
  mode = mode_vol & 0xf0;
  set_sum_mix();

  vol = mode_vol & 0x0f;
}

// Set filter cutoff frequency.
void Filter::set_w0()
{
  {
    // MOS 6581
    model_filter_t& f = model_filter[0];
    int Vw = Vw_bias + f.f0_dac[fc];
    Vddt_Vw_2 = unsigned(f.kVddt - Vw)*unsigned(f.kVddt - Vw) >> 1;
  }

  {
    // MOS 8580 cutoff: 0 - 12.5kHz.
    model_filter_t& f = model_filter[1];
    n_dac = (n_param * f.f0_dac[fc]) >> 15;
  }
}

// Set input routing bits.
void Filter::set_sum_mix()
{
  // NB! voice3off (mode bit 7) only affects voice 3 if it is routed directly
  // to the mixer.
  sum = (enabled ? filt : 0x00) & voice_mask;
  mix =
    (enabled ? (mode & 0x70) | ((~(filt | (mode & 0x80) >> 5)) & 0x0f) : 0x0f)
    & voice_mask;
}

} // namespace reSID
