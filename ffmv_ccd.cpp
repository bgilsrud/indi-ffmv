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

const int POLLMS           = 500;       /* Polling interval 500 ms */

std::auto_ptr<FFMVCCD> ffmvCCD(0);

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
    FlyCapture2::Error error;
    FlyCapture2::PGRGuid guid;
    FlyCapture2::CameraInfo cam_info;
    FlyCapture2::BusManager bus_man;
    FlyCapture2::Format7Info fmt7_info;
    FlyCapture2::Format7ImageSettings fmt7_settings;
    FlyCapture2::Format7PacketInfo fmt7_pkt_info;
    bool supported;
    bool settings_valid;

    error = bus_man.GetCameraFromIndex(0, &guid);
    if (error != FlyCapture2::PGRERROR_OK)
    {
        IDMessage(getDeviceName(), "Could not find FireFly MV");
        return false;
    }

    error = m_cam.Connect(&guid);
    if (error != FlyCapture2::PGRERROR_OK)
    {
        IDMessage(getDeviceName(), "Could not connect to FireFly MV");
        return false;
    }

    // Get the camera information
    error = m_cam.GetCameraInfo(&cam_info);
    if (error != FlyCapture2::PGRERROR_OK)
    {
        IDMessage(getDeviceName(), "Could not get FireFly camera properties");
        return false;
    }

    fmt7_info.mode = FlyCapture2::MODE_0;
    fmt7_info.pixelFormatBitField = FlyCapture2::PIXEL_FORMAT_MONO16;
    error = m_cam.GetFormat7Info(&fmt7_info, &supported);
    if (error != FlyCapture2::PGRERROR_OK)
    {
        IDMessage(getDeviceName(), "Could not get FireFly Format 7 info");
        return false;
    }
    if (!supported) {
        IDMessage(getDeviceName(), "Format 7 settings are not supported");
        return false;
    }

    fmt7_settings.mode = FlyCapture2::MODE_0;
    fmt7_settings.offsetX = 0;
    fmt7_settings.offsetY = 0;
    fmt7_settings.width = fmt7_info.maxWidth;
    fmt7_settings.height = fmt7_info.maxHeight;
    fmt7_settings.pixelFormat = FlyCapture2::PIXEL_FORMAT_MONO16;

    error = m_cam.ValidateFormat7Settings(&fmt7_settings, &settings_valid, &fmt7_pkt_info);
    if (error != FlyCapture2::PGRERROR_OK) {
        IDMessage(getDeviceName(), "Could not validate Format 7 info");
        return false;
    }
    if (!settings_valid) {
        IDMessage(getDeviceName(), "Invalid Format 7 settings");
        return false;
    }
    error = m_cam.SetFormat7Configuration(&fmt7_settings, fmt7_pkt_info.recommendedBytesPerPacket);
    IDMessage(getDeviceName(), "Successfully connected to FireFly MV");
    if (error != FlyCapture2::PGRERROR_OK) {
        IDMessage(getDeviceName(), "Could not set Format 7 settings");
        return false;
    }

    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool FFMVCCD::Disconnect()
{
    FlyCapture2::Error error;

    error = m_cam.Disconnect();
    if (error != FlyCapture2::PGRERROR_OK) {
        return false;
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
    SetCCDParams(752, 480, 16, 6.0, 6.0);

    // Let's calculate how much memory we need for the primary CCD buffer
    int nbuf;
    nbuf=PrimaryCCD.getXRes()*PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8;
    PrimaryCCD.setFrameBufferSize(nbuf);
}

/**************************************************************************************
** Client is asking us to start an exposure
***************************************************************************************/
int FFMVCCD::StartExposure(float duration)
{
    FlyCapture2::Error error;
    FlyCapture2::Property prop(FlyCapture2::AUTO_EXPOSURE);
    FlyCapture2::PropertyInfo prop_info;
    int ms;
    unsigned int val;
    float gain = 1.0;

    ms = duration* 1000;

    // Set framerate
    prop_info.type = FlyCapture2::FRAME_RATE;
    m_cam.GetPropertyInfo(&prop_info);
    prop.type = FlyCapture2::FRAME_RATE;
    prop.onOff = true;
    prop.autoManualMode = false;
    prop.absControl = true;
    prop.absValue = prop_info.absMin;
    error = m_cam.SetProperty(&prop);
    if (error != FlyCapture2::PGRERROR_OK) {
        IDMessage(getDeviceName(), "Failed to set framerate: %s", error.GetDescription());
        return -1;
    }

    //Enable extended shutter times
    error = m_cam.ReadRegister(0x83C, &val);
    if (error != FlyCapture2::PGRERROR_OK) {
        IDMessage(getDeviceName(), "Failed to read FRAME_RATE register: %s", error.GetDescription());
        return -1;
    }
    val &= ~(1 << 6);
    error = m_cam.WriteRegister(0x83C, val);
    if (error != FlyCapture2::PGRERROR_OK) {
        IDMessage(getDeviceName(), "Failed to write FRAME_RATE register: %s", error.GetDescription());
        return -1;
    }

    // Turn off AutoExposure
    prop.type = FlyCapture2::AUTO_EXPOSURE;
    prop.onOff = false;
    prop.autoManualMode = false;
    prop.absControl = false;
    error = m_cam.SetProperty(&prop);
    if (error != FlyCapture2::PGRERROR_OK) {
        IDMessage(getDeviceName(), "Failed to disable autoexposure: %s", error.GetDescription());
        return -1;
    }

    // Set gain to high level
    prop_info.type = FlyCapture2::GAIN;
    m_cam.GetPropertyInfo(&prop_info);
    float current = gain * (prop_info.absMax - prop_info.absMin) + prop_info.absMin;
    prop.type = FlyCapture2::GAIN;
    prop.onOff = true;
    prop.autoManualMode = false;
    prop.absControl = true;
    prop.absValue = current;
    error = m_cam.SetProperty(&prop);
    if (error != FlyCapture2::PGRERROR_OK) {
        IDMessage(getDeviceName(), "Failed to set gain: %s", error.GetDescription());
        return -1;
    }

    // set brightness
    prop_info.type = FlyCapture2::BRIGHTNESS;
    m_cam.GetPropertyInfo(&prop_info);
    prop.type = FlyCapture2::BRIGHTNESS;
    prop.onOff = true;
    prop.autoManualMode = false;
    prop.absControl = false;
    prop.valueA = (0.55 * (prop_info.max - prop_info.min) + prop_info.min);
    error = m_cam.SetProperty(&prop);
    if (error != FlyCapture2::PGRERROR_OK) {
        IDMessage(getDeviceName(), "Failed to set brightness: %s", error.GetDescription());
        return -1;
    }
 

#if 0
    m_cam.ReadRegister(0x1048, &val);
    val &= 0xfffffffe;
    m_cam.WriteRegister(0x1048, val);
    #endif

    // Disable Gamma correction
    prop.type = FlyCapture2::GAMMA;
    prop.onOff = true;
    prop.autoManualMode = false;
    prop.absControl = false;
    prop.valueA = 0;
    error = m_cam.SetProperty(&prop);
    if (error != FlyCapture2::PGRERROR_OK) {
        IDMessage(getDeviceName(), "Failed to disable Gamma correction: %s", error.GetDescription());
        return -1;
    }

    prop.type = FlyCapture2::SHUTTER;
    prop_info.type = FlyCapture2::SHUTTER;
    m_cam.GetPropertyInfo(&prop_info);
    float absShutter = (float) (1000 * duration < prop_info.absMax ? 1000 * duration : prop_info.absMax);
    prop.onOff = true;
    prop.autoManualMode = false;
    prop.absControl = true;
    prop.absValue = absShutter;
    error = m_cam.SetProperty(&prop);
    if (error != FlyCapture2::PGRERROR_OK) {
        IDMessage(getDeviceName(), "Failed to set exposure length: %s", error.GetDescription());
        return -1;
    }


    // Start capturing images
    if (!capturing) {
            capturing = true;
            error = m_cam.StartCapture();
            if (error != FlyCapture2::PGRERROR_OK)
            {
                    IDMessage(getDeviceName(), "Failed to start image capture. %s", error.GetDescription());

                    return -1;
            }
    }

    ExposureRequest=duration;

    // Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
    PrimaryCCD.setExposureDuration(duration);

    gettimeofday(&ExpStart,NULL);

    InExposure=true;
    IDMessage(getDeviceName(), "Exposure has begun.");

    // We're done
    return 0;
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
   FlyCapture2::Error error;
   FlyCapture2::Image raw_image;
   FlyCapture2::Image converted_image;
   unsigned char *myimage;

   // Let's get a pointer to the frame buffer
   char * image = PrimaryCCD.getFrameBuffer();

   // Get width and height
   int width = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
   int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

   //memset(image, 0, width * height * PrimaryCCD.getBPP());

   // Retrieve an image
   error = m_cam.RetrieveBuffer(&raw_image);
   if (error != FlyCapture2::PGRERROR_OK)
   {
       error.PrintErrorTrace();
       IDMessage(getDeviceName(), "Could not convert image");
        return;
   }

#if 0
   // Convert the raw image
   error = raw_image.Convert(FlyCapture2::PIXEL_FORMAT_MONO16, &converted_image);
   if (error != FlyCapture2::PGRERROR_OK)
   {
       error.PrintErrorTrace();
       IDMessage(getDeviceName(), "Could not convert image");
       return;
   } 
   #endif



   myimage = raw_image.GetData();
   // Fill buffer with random pattern
   for (int i=0; i < height ; i++) {
       for (int j=0; j < width; j++) {
           ((uint16_t *) image)[i*width+j] = *(raw_image(i, j));
           //((uint16_t *) image)[i*width+j] += *(raw_image(i, j));
       }
   }

   IDMessage(getDeviceName(), "Download complete.");

   // Let INDI::CCD know we're done filling the image buffer
   ExposureComplete(&PrimaryCCD);
}
