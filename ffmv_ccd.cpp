/**
 * Copyright (C) 2013 Ben Gilsrud
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <sys/time.h>
#include <memory>
#include <stdint.h>
#include <arpa/inet.h>
#include <math.h>

#include <indiapi.h>
#include <iostream>
#include "ffmv_ccd.h"
#include <dc1394/dc1394.h>

const int POLLMS = 250;

typedef union {
    uint32_t raw_val;
    float fval;
} abs_csr;

std::auto_ptr<FFMVCCD> ffmvCCD(0);

/**
 * FlyCapture doesn't provide an API to write to registers in the MT9V022 chip.
 * This can be done by programming the address in 0x1A00 and writing to 0x1A00.
 */
FlyCapture2::Error FFMVCCD::writeMicronReg(unsigned int offset, unsigned int val)
{
        FlyCapture2::Error error;
        unsigned int rd;

        error = m_cam.WriteRegister(0x1A00, offset);
        if (error != FlyCapture2::PGRERROR_OK) {
                return error;
        }

        error = m_cam.ReadRegister(0x1A04, &rd);
        if (error != FlyCapture2::PGRERROR_OK) {
                return error;
        }
        IDMessage(getDeviceName(), "Micro reg 0x%x before: 0x%x", offset, rd);

        error = m_cam.WriteRegister(0x1A04, val);

        error = m_cam.ReadRegister(0x1A04, &rd);
        if (error != FlyCapture2::PGRERROR_OK) {
                return error;
        }
        IDMessage(getDeviceName(), "Micro reg 0x%x after: 0x%x", offset, rd);
        return error;
}

FlyCapture2::Error FFMVCCD::readMicronReg(unsigned int offset, unsigned int *val)
{
        FlyCapture2::Error error;

        error = m_cam.WriteRegister(0x1A00, offset);
        if (error != FlyCapture2::PGRERROR_OK) {
                return error;
        }

        error = m_cam.ReadRegister(0x1A04, val);
        if (error != FlyCapture2::PGRERROR_OK) {
                return error;
        }

        return error;
}
void ISInit()
{
    static int isInit =0;
    if (isInit == 1)
        return;

     isInit = 1;
     if(ffmvCCD.get() == 0) ffmvCCD.reset(new FFMVCCD());
}

void ISGetProperties(const char *dev)
{
         ISInit();
         ffmvCCD->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
         ISInit();
         ffmvCCD->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
         ISInit();
         ffmvCCD->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
         ISInit();
         ffmvCCD->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
   INDI_UNUSED(dev);
   INDI_UNUSED(name);
   INDI_UNUSED(sizes);
   INDI_UNUSED(blobsizes);
   INDI_UNUSED(blobs);
   INDI_UNUSED(formats);
   INDI_UNUSED(names);
   INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
     ISInit();
     ffmvCCD->ISSnoopDevice(root);
}


FFMVCCD::FFMVCCD()
{
    InExposure = false;
    capturing = false;
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool FFMVCCD::Connect()
{
    dc1394camera_list_t *list;
    dc1394error_t err;
    bool supported;
    bool settings_valid;
    unsigned int val;
    dc1394format7mode_t fm7;
    dc1394feature_info_t feature;
    float min, max;

    dc1394 = dc1394_new();
    if (!dc1394) {
        return false;
    }

    err = dc1394_camera_enumerate(dc1394, &list);
    if (err != DC1394_SUCCESS) {
        IDMessage(getDeviceName(), "Could not find DC1394 cameras!");
        return false;
    }
    if (!list->num) {
        IDMessage(getDeviceName(), "No DC1394 cameras found!");
        return false;
    }
    dcam = dc1394_camera_new(dc1394, list->ids[0].guid);
    if (!dcam) {
        IDMessage(getDeviceName(), "Unable to connect to camera!");
        return false;
    }

    err = dc1394_video_set_mode(dcam, DC1394_VIDEO_MODE_640x480_MONO16);
    if (err != DC1394_SUCCESS) {
        IDMessage(getDeviceName(), "Unable to connect to set videomode!");
        return false;
    }
    /* Disable Auto exposure control */
    err = dc1394_feature_set_power(dcam, DC1394_FEATURE_EXPOSURE, DC1394_OFF);
    if (err != DC1394_SUCCESS) {
        IDMessage(getDeviceName(), "Unable to disable auto exposure control");
        return false;
    }

    /* Set frame rate to the lowest possible */
    err = dc1394_video_set_framerate(dcam, DC1394_FRAMERATE_7_5);
    if (err != DC1394_SUCCESS) {
        IDMessage(getDeviceName(), "Unable to connect to set framerate!");
        return false;
    }
    /* Turn frame rate control off to enable extended exposure (subs of 512ms) */
    err = dc1394_feature_set_power(dcam, DC1394_FEATURE_FRAME_RATE, DC1394_OFF);
    if (err != DC1394_SUCCESS) {
        IDMessage(getDeviceName(), "Unable to disable framerate!");
        return false;
    }

    /* Get the longest possible exposure length */
    err = dc1394_feature_set_absolute_control(dcam, DC1394_FEATURE_SHUTTER, DC1394_ON);
    if (err != DC1394_SUCCESS) {
        IDMessage(getDeviceName(), "Failed to enable ansolute shutter control.");
    } 
    err = dc1394_feature_get_absolute_boundaries(dcam, DC1394_FEATURE_SHUTTER, &min, &max);
    if (err != DC1394_SUCCESS) {
        IDMessage(getDeviceName(), "Could not get max shutter length");
    } else {
        max_exposure = max;
    }

    err=dc1394_capture_setup(dcam,10, DC1394_CAPTURE_FLAGS_DEFAULT);

    //Vref signal boost
    //error = writeMicronReg(0x2C, 0x00);

    //Digital gain boost
    //error = m_cam.WriteRegister(0x820, 0x7F);

    //2X gain boost
    //error = writeMicronReg(0x80, 0xF8);


    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool FFMVCCD::Disconnect()
{
    if (dcam) {
        dc1394_capture_stop(dcam);
        dc1394_camera_free(dcam);
    }

    IDMessage(getDeviceName(), "Point Grey FireFly MV disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char * FFMVCCD::getDefaultName()
{
    return "Point Grey FireFly MV";
}

/**************************************************************************************
** INDI is asking us to init our properties.
***************************************************************************************/
bool FFMVCCD::initProperties()
{
    // Must init parent properties first!
    INDI::CCD::initProperties();

    // Add Debug, Simulator, and Configuration controls
    addAuxControls();

    return true;

}

/**************************************************************************************
** INDI is asking us to submit list of properties for the device
***************************************************************************************/
void FFMVCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

}

/********************************************************************************************
** INDI is asking us to update the properties because there is a change in CONNECTION status
** This fucntion is called whenever the device is connected or disconnected.
*********************************************************************************************/
bool FFMVCCD::updateProperties()
{
    // Call parent update properties first
    INDI::CCD::updateProperties();

    if (isConnected())
    {

        // Let's get parameters now from CCD
        setupParams();

        // Start the timer
        SetTimer(POLLMS);
    }

    return true;
}

/**************************************************************************************
** Setting up CCD parameters
***************************************************************************************/
void FFMVCCD::setupParams()
{
    // The FireFly MV has a Micron MT9V022 CMOS sensor
    SetCCDParams(640, 480, 16, 6.0, 6.0);

    // Let's calculate how much memory we need for the primary CCD buffer
    int nbuf;
    nbuf=PrimaryCCD.getXRes()*PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8;
    PrimaryCCD.setFrameBufferSize(nbuf);
}

#define IMAGE_FILE_NAME "testimage.pgm"
/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
bool FFMVCCD::StartExposure(float duration)
{
    FILE *imagefile;
    dc1394error_t err;
    dc1394video_frame_t *frame;
    int i;
    int ms;
    unsigned int val;
    float gain = 1.0;
    uint32_t uwidth, uheight;
    float sub_length;

    ms = duration* 1000;

    //IDMessage(getDeviceName(), "Doing %d sub exposures at %f %s each", sub_count, absShutter, prop_info.pUnits);

    ExposureRequest=duration;

    // Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
    PrimaryCCD.setBPP(16);
    PrimaryCCD.setExposureDuration(duration);

    gettimeofday(&ExpStart,NULL);

    InExposure=true;
    IDMessage(getDeviceName(), "Exposure has begun.");

    // Let's get a pointer to the frame buffer
    char * image = PrimaryCCD.getFrameBuffer();

    // Get width and height
    int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

    memset(image, 0, PrimaryCCD.getFrameBufferSize());

    if (duration != last_exposure_length) {
        /* Calculate the number of exposures needed */
        sub_count = duration / max_exposure;
        if (ms % ((int) (max_exposure * 1000))) {
            ++sub_count;
        }
        sub_length = duration / sub_count;

        IDMessage(getDeviceName(), "Triggering a %f second exposure using %d subs of %f seconds",
                duration, sub_count, sub_length);
        /* Set sub length */
        err = dc1394_feature_set_absolute_value(dcam, DC1394_FEATURE_SHUTTER, sub_length);
        if (err != DC1394_SUCCESS) {
            IDMessage(getDeviceName(), "Unable to set shutter value.");
        }
    }

    /*-----------------------------------------------------------------------
     *  have the camera start sending us data
     *-----------------------------------------------------------------------*/
    IDMessage(getDeviceName(), "start transmission");
    err=dc1394_video_set_transmission(dcam, DC1394_ON);
    if (err != DC1394_SUCCESS) {
            IDMessage(getDeviceName(), "Unable to start transmission");
            return false;
    }

    // We're done
    return true;
}

/**************************************************************************************
** Client is asking us to abort an exposure
***************************************************************************************/
bool FFMVCCD::AbortExposure()
{
    InExposure = false;
    return true;
}

/**************************************************************************************
** How much longer until exposure is done?
***************************************************************************************/
float FFMVCCD::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec/1000);
    timesince=timesince/1000;

    timeleft=ExposureRequest-timesince;
    return timeleft;
}

/**************************************************************************************
** Client is asking us to set a new number
***************************************************************************************/
bool FFMVCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    INumber *np;

    if(strcmp(dev,getDeviceName())==0)
    {

    }

    // If we didn't process anything above, let the parent handle it.
    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}

/**************************************************************************************
** INDI is asking us to add any FITS keywords to the FITS header
***************************************************************************************/
void FFMVCCD::addFITSKeywords(fitsfile *fptr, CCDChip *targetChip)
{
    // Let's first add parent keywords
    INDI::CCD::addFITSKeywords(fptr, targetChip);

}

/**************************************************************************************
** Main device loop. We check for exposure progress
***************************************************************************************/
void FFMVCCD::TimerHit()
{
    long timeleft;

    if(isConnected() == false)
        return;  //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft=CalcTimeLeft();

        // Less than a 0.1 second away from exposure completion
        // This is an over simplified timing method, check CCDSimulator and ffmvCCD for better timing checks
        if(timeleft < 0.1)
        {
          /* We're done exposing */
           IDMessage(getDeviceName(), "Exposure done, downloading image...");

          // Set exposure left to zero
          PrimaryCCD.setExposureLeft(0);

          // We're no longer exposing...
          InExposure = false;

          /* grab and save image */
          grabImage();

        }
        else
         // Just update time left in client
         PrimaryCCD.setExposureLeft(timeleft);

    }

    SetTimer(POLLMS);
    return;
}

/**
 * Download image from FireFly
 */
void FFMVCCD::grabImage()
{
   unsigned char *myimage;
   dc1394error_t err;
   dc1394video_frame_t *frame;
   uint32_t uheight, uwidth;
   int sub;
   uint16_t val;

   // Let's get a pointer to the frame buffer
   char * image = PrimaryCCD.getFrameBuffer();

   // Get width and height
   int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
   int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

   memset(image, 0, PrimaryCCD.getFrameBufferSize());


   /*-----------------------------------------------------------------------
    *  stop data transmission
    *-----------------------------------------------------------------------*/
   err=dc1394_video_set_transmission(dcam,DC1394_OFF);


   for (sub = 0; sub < sub_count; ++sub) {
       IDMessage(getDeviceName(), "Getting sub %d of %d", sub, sub_count);
       err=dc1394_capture_dequeue(dcam, DC1394_CAPTURE_POLICY_WAIT, &frame);
       if (err != DC1394_SUCCESS) {
               IDMessage(getDeviceName(), "Could not capture frame");
       }
       dc1394_get_image_size_from_video_mode(dcam,DC1394_VIDEO_MODE_640x480_MONO16, &uwidth, &uheight);

       // Fill buffer with random pattern
       for (int i=0; i < height ; i++) {
           for (int j=0; j < width; j++) {
               val = ((uint16_t *) image)[i*width+j] + ntohs(((uint16_t*) (frame->image))[i*width+j]);
               if (val > ((uint16_t *) image)[i*width+j]) {
                   ((uint16_t *) image)[i*width+j] = val;
               } else {
                   ((uint16_t *) image)[i*width+j] = 0xFFFF;
               }
               //image[i*width+j] += frame->image[i*width+j];
           }
       }

       dc1394_capture_enqueue(dcam, frame);
   }
   IDMessage(getDeviceName(), "Download complete.");

   // Let INDI::CCD know we're done filling the image buffer
   ExposureComplete(&PrimaryCCD);
}
