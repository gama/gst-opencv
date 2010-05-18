#include <fstream>

#include <math.h>
#include <string.h>
#include <time.h>

#include <cv.h>
#include <highgui.h>

#include "OS_specific.h"
#if OS_type==2
#include <conio.h>
#include <io.h>   
#endif

#include <sys/types.h> 
#include <sys/stat.h> 

#include "Patches.h"
#include "ImageRepresentation.h"

#include "StrongClassifierDirectSelection.h"
#include "StrongClassifierStandard.h"
#include "Detector.h"
#include "Classifier.h"

#include "ImageSource.h"
#include "ImageSourceDir.h"
#include "ImageHandler.h"
#include "ImageSourceUSBCam.h"
#include "ImageSourceAVIFile.h"

int mouse_pointX;
int mouse_pointY;
int mouse_value;
bool mouse_exit;
Rect trackingRect;

bool keyboard_pressed = false;

int readConfigFile(const char* configFile, ImageSource::InputDevice& input, int& numBaseClassifiers, float& overlap, float& searchFactor, char* resultDir, char* source, Rect& initBB) {
    printf("parsing config file %s\n", configFile);

    fstream f;
    char cstring[1000];
    int readS = 0;
    f.open(configFile, fstream::in);
    if (!f.eof()) {
        //version
        //skip first line
        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        char param1[200];
        strcpy(param1, "");
        char param2[200];
        strcpy(param2, "");
        char param3[200];
        strcpy(param3, "");

        readS = sscanf(cstring, "%s %s", param1, param2);
        char match[100];
        strcpy(match, "0.3");
        if (param2[2] != match[2]) {
            printf("ERROR: unsupported version of config file!\n");
            return -1;
        }
        printf("  %s %s\n", param1, param2);

        //source
        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        readS = sscanf(cstring, "  %s %s %s", param1, param2, param3);
        strcpy(match, "USB");
        if (param3[0] == match[0])
            input = ImageSource::USB;
        else {
            strcpy(match, "AVI");
            if (param3[0] == match[0])
                input = ImageSource::AVI;
            else {
                strcpy(match, "IMAGES");
                if (param3[0] == match[0])
                    input = ImageSource::DIRECTORY;
                else
                    return -1;
            }
        }
        printf("  %s %s %s\n", param1, param2, param3);
        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        //directory
        if (input != ImageSource::USB) {
            readS = sscanf(cstring, "%s %s %s", param1, param2, param3);
            strcpy(source, param3);

            printf("  %s %s %s\n", param1, param2, param3);

        }

        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        //debug information
        readS = sscanf(cstring, "  %s %s %s", param1, param2, param3);
        f.getline(cstring, sizeof (cstring));
        strcpy(match, "true");
        if (param3[0] == match[0]) {

            readS = sscanf(cstring, "  %s %s %s", param1, param2, param3);
            printf("  %s %s %s\n", param1, param2, param3);

            //check if result dir exists
            if (access(param3, 0) == 0) {
                struct stat status;
                stat(param3, &status);

                if (status.st_mode & S_IFDIR)
                    strcpy(resultDir, param3);
                else
                    resultDir = NULL;
            } else resultDir = NULL;
            if (resultDir == NULL)
                printf("    ERROR: resulDir does not exist - switch off debug\n");
        } else {
            resultDir = NULL;
        }

        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        //boosting parameters
        readS = sscanf(cstring, "%s %s %d", param1, param2, &numBaseClassifiers);
        printf("  %s %s %i\n", param1, param2, numBaseClassifiers);

        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        // search region
        readS = sscanf(cstring, "%s %s %f", param1, param2, &overlap);
        printf("  %s %s %5.3f\n", param1, param2, overlap);
        f.getline(cstring, sizeof (cstring));
        readS = sscanf(cstring, "%s %s %f", param1, param2, &searchFactor);
        printf("  %s %s %5.3f\n", param1, param2, searchFactor);

        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        f.getline(cstring, sizeof (cstring));
        // initialization bounding box: MOUSE or COORDINATES
        readS = sscanf(cstring, "  %s %s %s", param1, param2, param3);
        strcpy(match, "MOUSE");
        if (param3[0] == match[0])
            initBB = Rect(0, 0, 0, 0);
        else {
            strcpy(match, "COORDINATES");
            if (param3[0] == match[0]) {
                f.getline(cstring, sizeof (cstring));
                f.getline(cstring, sizeof (cstring));
                f.getline(cstring, sizeof (cstring));
                readS = sscanf(cstring, "%s %s %i %i %i %i", param1, param2, &initBB.left, &initBB.upper, &initBB.width, &initBB.height);
                printf("  %s %s %i %i %i %i\n", param1, param2, initBB.left, initBB.upper, initBB.width, initBB.height);
            }
        }
    } else
        return -1;

    f.close();
    printf("parsing done\n\n");


    return 0;
}

Rect getTrackingROI(float searchFactor, Rect rect, Rect validROI) {
    Rect searchRegion;

    searchRegion = rect * (searchFactor);
    //check
    if (searchRegion.upper + searchRegion.height > validROI.height)
        searchRegion.height = validROI.height - searchRegion.upper;
    if (searchRegion.left + searchRegion.width > validROI.width)
        searchRegion.width = validROI.width - searchRegion.left;

    return searchRegion;
}

void track3(ImageSource::InputDevice input, int numBaseClassifier, float overlap, float searchFactor, char* resultDir, Rect initBB, char* source = NULL)
{
	unsigned char *curFrame=NULL;
        int key;
	//choose the image source
	ImageSource *imageSequenceSource;
	switch (input)
	{
	case ImageSource::AVI:
		imageSequenceSource = new ImageSourceAVIFile(source);
		break;
	case ImageSource::DIRECTORY:
		imageSequenceSource = new ImageSourceDir(source);
		break;
	case ImageSource::USB:
		imageSequenceSource = new ImageSourceUSBCam();
		break;
	default:
		return;
	}

	ImageHandler* imageSequence = new ImageHandler (imageSequenceSource);
	imageSequence->getImage();

	imageSequence->viewImage ("Tracking...", false);

		trackingRect=initBB;


	curFrame = imageSequence->getGrayImage ();
	ImageRepresentation* curFrameRep = new ImageRepresentation(curFrame, imageSequence->getImageSize());
	Rect wholeImage;
	wholeImage = imageSequence->getImageSize();


        // Pula o inicio do video
        for(int t = 0; t < 60; t++, imageSequence->getImage());


        IplImage *image;
        image = imageSequence->getIplImage();


//cvWaitKey(0);


        printf ("init tracker...");
        Classifier* tracker;
        tracker = new Classifier(image, trackingRect);
        printf (" done.\n");

	Size trackingRectSize;
	trackingRectSize = trackingRect;
	printf ("start tracking (stop by pressing any key)...\n\n");

// Inicializa o detector
Detector* detector;
detector = new Detector(tracker->getClassifier());
Rect trackedPatch = trackingRect;
Rect validROI;
validROI.upper = validROI.left = 0;
validROI.height = image->height;
validROI.width = image->width;


	key=(char)-1;
        while (key==(char)-1)
	{





		imageSequence->getImage();
                image = imageSequence->getIplImage();




curFrame = imageSequence->getGrayImage ();
if (curFrame == NULL) break;

//calculate the patches within the search region
Patches *trackingPatches;
Rect searchRegion;
searchRegion = getTrackingROI(searchFactor, trackedPatch, validROI);
trackingPatches = new PatchesRegularScan(searchRegion, wholeImage, trackingRectSize, overlap);

curFrameRep->setNewImageAndROI(curFrame, searchRegion);

detector->classifySmooth(curFrameRep, trackingPatches);

trackedPatch = trackingPatches->getRect(detector->getPatchIdxOfBestDetection());

if (detector->getNumDetections() <= 0){printf("Lost...\n");break;}



                // Treina o classificador
                tracker->train(image,trackedPatch);


float alpha, confidence, eval;

alpha = tracker->getSumAlphaClassifier();
confidence = detector->getConfidenceOfBestDetection() / alpha;
eval = tracker->classify(image, trackedPatch);

printf("alpha: %5.3f confidence: %5.3f evalOficial: %5.3f ", alpha, confidence, eval);

int orig = trackedPatch.upper;
trackedPatch.upper -= 5;
for(int i = 0; i < 10; i++){
    eval = tracker->classify(image, trackedPatch);
    printf("%5.3f ", eval);
    trackedPatch.upper += 1;
    imageSequence->paintRectangle (trackedPatch, Color (0,255,0), 1);
}
trackedPatch.upper = orig;

printf("\n");




		//display results
                imageSequence->paintRectangle (trackedPatch, Color (255,0,0), 5);
		imageSequence->viewImage ("Tracking...", false);
		key=cvWaitKey(200);
	}

	//clean up
	delete tracker;
	delete imageSequenceSource;
	delete imageSequence;
	if (curFrame == NULL)
		delete[] curFrame;
	delete curFrameRep;
}

int main(int argc, char* argv[]) {

    printf("-------------------------------------------------------\n");
    printf("                   Classifier Tracker                    \n");
    printf("-------------------------------------------------------\n\n");

    ImageSource::InputDevice input;
    input = ImageSource::USB;

    int numBaseClassifier;
    char* source;
    char* resultDir;
    float overlap, searchFactor;
    Rect initBB;

    resultDir = new char[100];
    memset(resultDir, '\0', 100);
    source = new char[100];
    memset(source, '\0', 100);
    initBB = Rect(0, 0, 0, 0);

    //read parameters from config file
    int ret;
    if (argc >= 2)
        ret = readConfigFile(argv[1], input, numBaseClassifier, overlap, searchFactor, resultDir, source, initBB);
    else
        ret = readConfigFile("./config.txt", input, numBaseClassifier, overlap, searchFactor, resultDir, source, initBB);

    if (ret < 0) {
        printf("ERROR: config file damaged\n");
        return -1;
    }

    //start tracking
    track3(input, numBaseClassifier, overlap, searchFactor, resultDir, initBB, source);

    return 0;
}


