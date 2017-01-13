/*
 * Do not edit this file.
 * It has been generated by TwiXt
 * from widget description Joystick.xt.
 */
#ifndef JOYSTICKP_H
#define JOYSTICKP_H

#include <X11/IntrinsicP.h>

/* Include public header */
#include <Joystick.h>

/* Include private header of superclass */
#ifdef USE_XAW3D
#include <X11/Xaw3d/SimpleP.h>
#else
#include <X11/Xaw/SimpleP.h>
#endif

/* New representation types used by the Joystick widget */

/* Declarations for class functions */
extern void JoystickClassInitialize(void);
extern void JoystickClassPartInitialize(WidgetClass /* class */);
extern void JoystickInitialize(Widget /* request */, Widget /* new */, ArgList /* args */, Cardinal * /* num_args */);
extern void JoystickDestroy(Widget /* widget */);
extern void JoystickResize(Widget /* widget */);
extern void JoystickExpose(Widget /* widget */, XEvent* /* event */, Region /* region */);
extern Boolean JoystickSetValues(Widget /* old */, Widget /* request */, Widget /* new */, ArgList /* args */, Cardinal* /* num_args */);


/* Defines for inheriting superclass function pointer values */


/* New fields for the Joystick instance record */
typedef struct {
    /* Settable resources and private data */
    Pixel fire_color; 
    Pixel direction_color; 
    Pixel off_color; 
    Cardinal enable_bits; /* From lsb to msb: up down left right fire */
    int led_xsize; 
    int led_ysize; 
    GC gc; 

} JoystickPart;

/* Full instance record declaration */
typedef struct JoystickRec {
    CorePart core;
    SimplePart simple;
    JoystickPart joystick;

} JoystickRec;

/* Types for Joystick class methods */

/* New fields for the Joystick class record */
typedef struct {

} JoystickClassPart;

/* Class extension records */


/* Full class record declaration */
typedef struct JoystickClassRec {
    CoreClassPart core_class;
    SimpleClassPart simple_class;
    JoystickClassPart joystick_class;

} JoystickClassRec;

extern struct JoystickClassRec joystickClassRec;


#endif /* JOYSTICKP_H */
