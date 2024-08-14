#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <tesseract/capi.h>
#include <leptonica/allheaders.h>
#include <unistd.h>
#include <string.h>

#define POSX   0
#define POSY   0
#define HEIGHT 200
#define WIDTH  600
#define BORDER 15 

enum Actions {
    BLOCK,   
    ITEM_FOUND,
    UNWANTED_ITEM,
    ESCAPE,
    START_COMBAT,
    ATTACK,
    CHANGE_POKEMON,
    START_FISHING,
    ATTACK_LUV,
    REMOVE_OBJECT,
};

bool blockCompleted = false;
bool isInCombat = false;
bool wasItemFound = false;
bool firstTimeAttacking = true;
bool gotItem = false;
bool isReadyToFight = false;
Window window = 0x1c00007; 
 
void restartState(){ 
    blockCompleted = false;
    isInCombat = false;
    wasItemFound = false;
    firstTimeAttacking = true;
}

Window find_window_by_name(Display *display, Window root, const char *window_name) {
    Window parent;
    Window *children;
    unsigned int nchildren;
    char *name;

    // Get the list of child windows
    if (XQueryTree(display, root, &root, &parent, &children, &nchildren)) {
        for (unsigned int i = 0; i < nchildren; i++) {
            // Get the window name
            if (XFetchName(display, children[i], &name) > 0) {
                // Compare the name
                printf("%s\n", name);
                if (name && strcmp(name, window_name) == 0) {
                    XFree(name);
                    return children[i]; // Return the matched window ID
                }
                XFree(name);
            }

            // Recursively search in child windows
            Window w = find_window_by_name(display, children[i], window_name);
            if (w) {
                return w;
            }
        }

        if (children) {
            XFree(children); // Free the list of child windows
        }
    }

    return 0; // Return 0 if no matching window is found
}

void save_image_as_ppm(const char *filename, XImage *image) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error opening file");
        return;
    }

    fprintf(file, "P6\n%d %d\n255\n", image->width, image->height);

    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            unsigned long pixel = XGetPixel(image, x, y);
            fputc((pixel >> 16) & 0xFF, file); 
            fputc((pixel >> 8) & 0xFF, file);  
            fputc(pixel & 0xFF, file);         
        }
    }

    fclose(file);
}

int captureScreen(Display *display, int posX, int posY, unsigned width, unsigned height, char* imgName){

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    int screenHeight = DisplayHeight(display, screen);

    XImage *image = XGetImage(display, root, posX, posY, width, height , AllPlanes, ZPixmap);
    if (image == NULL) {
        fprintf(stderr, "Failed to get image\n");
        XCloseDisplay(display);
        return 1;
    }

    save_image_as_ppm(imgName, image);

    XDestroyImage(image);
    return 0;

}

char *readImageText(char* imgName){
    // Initialize Tesseract API
    TessBaseAPI *api = TessBaseAPICreate();
    if (TessBaseAPIInit3(api, NULL, "eng")) {
        fprintf(stderr, "Could not initialize tesseract.\n");
        TessBaseAPIDelete(api);
        return NULL;
    }

    // Read the image using Leptonica
    struct Pix *pixImage = pixRead(imgName);
    if (!pixImage) {
        fprintf(stderr, "Could not read image.\n");
        TessBaseAPIDelete(api);
        return NULL;
    }

    // Set the image to Tesseract API
    TessBaseAPISetImage2(api, pixImage);

    char *text = TessBaseAPIGetUTF8Text(api);
    size_t length = strlen(text);
    char *copy = malloc((length + 1) * sizeof(char));

    if (copy == NULL) {
        return NULL; // Handle malloc failure
    }
    strcpy(copy, text);

    if (text) {
        printf("\nOCR Output:\n%s", text);
        TessDeleteText(text);
    } else {
        fprintf(stderr, "Failed to get OCR text.\n");
    }

    // Cleanup
    pixDestroy(&pixImage);
    TessBaseAPIDelete(api);
    return copy;
}

enum Actions getActionToExecute(char *text){
    
    char* foundItem = "found";
    char* unwantedItem1 = "Roca";
    char* unwantedItem2 = "P.";
    char* escape = "Escapas";
    char* horde = "horde"; 
    char* stoleItem = "robado";

    if (!wasItemFound && isInCombat) {
        wasItemFound = strstr(text, foundItem) != NULL;
    }

    if (isInCombat && strstr(text, stoleItem) != NULL) {
        printf("\nITEM WAS STOLEN, LEAVING...\n");
        gotItem = true;
        return ESCAPE;
    }

    if (!isInCombat) {
        printf("\nSHOULD START COMBAT!! :(\n");
        return START_COMBAT;
    } 

    if (blockCompleted == 0 && wasItemFound && isInCombat) {
        printf("\nA ITEM WAS FOUND!!\n");
        return BLOCK;
    } 

    if (isInCombat && wasItemFound == false) {
        printf("\nHORDE! !:(\n");
        return ESCAPE;
    } 

    if (isInCombat && wasItemFound) {
        printf("\nHORDE! !:(\n");
        return ATTACK;
    } 

    if (strstr(text, unwantedItem1) != NULL | strstr(text, unwantedItem2) != NULL) {
        printf("\nGOT UNWATED ITEM :(\n");
        return UNWANTED_ITEM;
    } 

    return 0;
}


enum Actions getActionToExecuteFishing(char *text){
    
    char* stoleItem = "le ha";
    char* luvdisc = "frisked Luvdisc";
    char* combatStarted = "aBanette";

    if (strstr(text, combatStarted) != NULL && isReadyToFight) {
        isInCombat = true;
    }

    if (strstr(text, stoleItem) != NULL && !isInCombat) {
        return REMOVE_OBJECT;
    }


    if (isInCombat && strstr(text, stoleItem) != NULL) {
        printf("\nITEM WAS STOLEN, LEAVING...\n");
        gotItem = true;
        return ESCAPE;
    }

    if (!isInCombat && !isReadyToFight) {
        printf("\nSHOULD FISHING COMBAT!! \n");
        return START_FISHING;
    } 

    if (isInCombat && wasItemFound == false) {
        printf("\nEscaping!!\n");
        return ESCAPE;
    } 

    if (isInCombat && wasItemFound) {
        printf("\nHORDE! !:(\n");
        return ATTACK_LUV;
    } 

    return 0;
}

void simulateKeyPressFocusWin(Display *display, KeySym key) {
    KeyCode keycode = XKeysymToKeycode(display, key);
    
    // Press the key
    XTestFakeKeyEvent(display, keycode, True, 0);
    XFlush(display);

    usleep(80000);
    // Release the key
    XTestFakeKeyEvent(display, keycode, False, 0);
    XFlush(display);
}

void simulateKeyPressFocusWinDelay(int delay, Display *display, KeySym key) {
    KeyCode keycode = XKeysymToKeycode(display, key);
    
    // Press the key
    XTestFakeKeyEvent(display, keycode, True, 0);
    XFlush(display);

    usleep(delay);
    // Release the key
    XTestFakeKeyEvent(display, keycode, False, 0);
    XFlush(display);
}

Window get_focused_window(Display *display) {
    Window focused_window;
    int revert_to;
    
    // Get the currently focused window
    XGetInputFocus(display, &focused_window, &revert_to);
    
    return focused_window;
}

//void simulateKeyPressFocusWin(Display *display, KeySym key) {
//    // Window activeWindow = get_focused_window(display);
//    XKeyEvent event;
//    event.display = display;
//    event.window = window;
//    event.root = DefaultRootWindow(display);
//    event.subwindow = None;
//    event.time = CurrentTime;
//    event.x = 0;
//    event.y = 0;
//    event.x_root = 0;
//    event.y_root = 0;
//    event.same_screen = True;
//    event.state = 0;
//    event.keycode = XKeysymToKeycode(display, key);
//    event.type = KeyPress;
//
//    // XSetInputFocus(display, window, RevertToNone, CurrentTime);
//    XSendEvent(display, window, True, KeyPressMask, (XEvent *)&event);
//    XFlush(display);
//    usleep(40000);
//    event.type = KeyRelease;
//    XSendEvent(display, window, True, KeyReleaseMask, (XEvent *)&event);
//    // XSetInputFocus(display, activeWindow, RevertToNone, CurrentTime);
//    XFlush(display);
//}


void executeAction(Display *display, enum Actions action) {
    switch (action) {
        case BLOCK:
            printf("Starting block action.\n");
            simulateKeyPressFocusWin(display, XK_Z);
            simulateKeyPressFocusWin(display, XK_K); 
            simulateKeyPressFocusWin(display, XK_Z);
            simulateKeyPressFocusWin(display, XK_Z);
            printf("Block action done.\n");
            blockCompleted = true;
            break;
        case UNWANTED_ITEM:
            printf("Starting unwanted action.\n");
            printf("Unwanted action done.\n");
            break;
        case ESCAPE:
            printf("Starting escape action.\n");
            simulateKeyPressFocusWin(display, XK_J); 
            simulateKeyPressFocusWin(display, XK_K); 
            simulateKeyPressFocusWin(display, XK_Z); 
            printf("Escape action done.\n");
            restartState();
            break;
        case START_COMBAT:
            printf("Starting combat action.\n");
            simulateKeyPressFocusWin(display, XK_3);
            printf("Start combat action done.\n");
            isInCombat = true;
            break;
        case START_FISHING:
            printf("Starting combat action.\n");
            simulateKeyPressFocusWin(display, XK_2);
            break;
        case ATTACK_LUV:
            printf("Starting attack action.\n");
            simulateKeyPressFocusWin(display, XK_Z);
            simulateKeyPressFocusWin(display, XK_Z);
            printf("attack action done.\n");
            break;
        case CHANGE_POKEMON:
            printf("Starting combat action.\n");
            simulateKeyPressFocusWin(display, XK_J); 
            simulateKeyPressFocusWin(display, XK_K); 
            simulateKeyPressFocusWin(display, XK_Z); 
            printf("Start combat action done.\n");
            isInCombat = true;
            break;
        case ATTACK:
            printf("Starting attack action.\n");
            simulateKeyPressFocusWin(display, XK_Z);
            if (firstTimeAttacking){
                simulateKeyPressFocusWin(display, XK_H);
                firstTimeAttacking = false;
            }
            simulateKeyPressFocusWin(display, XK_Z);
            simulateKeyPressFocusWin(display, XK_Z);
            printf("attack action done.\n");
            break;
        case REMOVE_OBJECT:
            printf("\nStarting removing action.\n");
            simulateKeyPressFocusWin(display, XK_D);
            simulateKeyPressFocusWin(display, XK_Z);
            simulateKeyPressFocusWin(display, XK_Z);
            simulateKeyPressFocusWin(display, XK_Z);
            simulateKeyPressFocusWin(display, XK_J); 
            simulateKeyPressFocusWin(display, XK_J); 
            simulateKeyPressFocusWin(display, XK_Z);
            gotItem = false;
            printf("\nremoving action done.\n");
        default:
            printf("Unknown action.\n");
            break;
    }
}


// exp
int main4() {

    Display *display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "Unable to open X display\n");
        return 1;
    }

    sleep(2);

    while (1) {

        simulateKeyPressFocusWin(display, XK_3);
        sleep(5);
        simulateKeyPressFocusWin(display, XK_Z);
        simulateKeyPressFocusWin(display, XK_Z);
        simulateKeyPressFocusWin(display, XK_Z);
        sleep(3);
    }

    XCloseDisplay(display);

    return 0;
    
}

// FISHING
int main2() {

    // Run i3-msg command
//    fp = popen("i3-msg -s /run/user/1000/i3/ipc-socket.2185 -t get_tree", "r");
//    if (fp == NULL) {
//        perror("popen");
//        return 1;
//    }

//xdotool windowfocus 56623111 key XK_J 
    // Window window = find_window_by_name(display, DefaultRootWindow(display), windowName);
    
    Display *display = XOpenDisplay(NULL);
    sleep(2);

    while (1) {

        printf("Starting fishing action.\n");
        simulateKeyPressFocusWin(display, XK_2);
        sleep(4);
        printf("Fishing completed, waiting for fishing dialog.\n");

        int fightScreenShot = captureScreen(display, 1920 + 450, 30, WIDTH, HEIGHT, "fishingDialog.ppm");
        char* fishingDialogText = readImageText("fishingDialog.ppm");
        if (fishingDialogText == NULL) {
            return 1;
        }

        if (strstr(fishingDialogText, "picado") == NULL) {
            printf("\nNothing captured\n");
            simulateKeyPressFocusWin(display, XK_Z);
            continue;
        }

        simulateKeyPressFocusWin(display, XK_Z);
        printf("Waiting dialogs\n");
        sleep(13);
        printf("Waiting dialogs END\n");

        int screenshotSuccess = captureScreen(display, 1920 + POSX, 906, WIDTH, 130, "chatScreenshot.ppm");
        if (screenshotSuccess != 0) {
            return 1;
        }

        char* text = readImageText("chatScreenshot.ppm");
        if (text == NULL) {
            return 1;
            break;
        }

        char* luvdisc = "frisked";
        if (strstr(text, luvdisc) == NULL) {
            printf("No item was found\n");
            executeAction(display, ESCAPE);
            sleep(4);
            continue;
        }
        printf("ITEM FOUND!!\n");

        executeAction(display, ATTACK_LUV);
        sleep(11);
        executeAction(display, REMOVE_OBJECT);
        free(text);
    }

    XCloseDisplay(display);

    return 0;
}

// HUEVOS
int main1() {

    Display *display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "Unable to open X display\n");
        return 1;
    }

    sleep(3);
    while (1) {

        simulateKeyPressFocusWin(display, XK_3);

        int fightScreenShot = captureScreen(display, 1920 + 300, 640, WIDTH, HEIGHT - 20, "readyToPlay.ppm");
        if (fightScreenShot != 0) {
            return 1;
            break;
        }

        char* fightText = readImageText("readyToPlay.ppm");
        if (fightText == NULL) {
            return 1;
            break;
        }

        isReadyToFight = strstr(fightText, "FIGHT BAG") != NULL;

        if (isReadyToFight || !isInCombat || gotItem) {

            int screenshotSuccess = captureScreen(display, 1920 + POSX, 830, WIDTH, HEIGHT, "chatScreenshot.ppm");
            sleep(1);

            if (screenshotSuccess != 0) {
                return 1;
                break;
            }

            sleep(1);
            char* text = readImageText("chatScreenshot.ppm");
            if (text == NULL) {
                return 1;
                break;
            }

            printf("LOOKING FOR ACTIONS TO EXECUTE");
            enum Actions action = getActionToExecute(text);
            executeAction(display, action);
            free(text);
            free(fightText);

        } else {
          printf("SKIPPING THIS TURN");
        }   

        sleep(3);
    }

    XCloseDisplay(display);

    return 0;
}

int main() {

    Display *display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "Unable to open X display\n");
        return 1;
    }

    sleep(3);
    while (1) {


        int fightScreenShot = captureScreen(display, 1920 + 300, 640, WIDTH, HEIGHT - 20, "readyToPlay.ppm");
        if (fightScreenShot != 0) {
            return 1;
            break;
        }

        char* fightText = readImageText("readyToPlay.ppm");
        if (fightText == NULL) {
            return 1;
            break;
        }

        sleep(2);
        isReadyToFight = strstr(fightText, "Troply") != NULL;

        if (isReadyToFight || strstr(fightText, "FIGHT") != NULL) {

            int screenshotSuccess = captureScreen(display, 1920 + POSX, 906, WIDTH, 130, "chatScreenshot.ppm");

            if (screenshotSuccess != 0) {
                return 1;
                break;
            }

            sleep(2);
            char* text = readImageText("chatScreenshot.ppm");
            if (text == NULL) {
                return 1;
                break;
            }
            if (strstr(text, "UnDitto") == NULL) {
                executeAction(display, ESCAPE);
                continue;
            }
            
            sleep(4);
            printf("DOING VOLTIOCAMBIO\n");
            simulateKeyPressFocusWin(display, XK_Z);
            simulateKeyPressFocusWin(display, XK_Z);
            sleep(12);
            printf("DOING VOLTIOCAMBIO DONE\n");

            printf("SWITCHING TO BRELOOM\n");
            simulateKeyPressFocusWin(display, XK_K);
            simulateKeyPressFocusWin(display, XK_Z);
            sleep(8);
            printf("SWITCHING TO BRELOOM DONE\n");


            // BREELOM
            printf("ESPORA\n");
            simulateKeyPressFocusWin(display, XK_H);
            simulateKeyPressFocusWin(display, XK_Z);
            simulateKeyPressFocusWin(display, XK_Z);
            sleep(14);
            printf("ESPORA DONE\n");


            printf("THORWING POKEBALL\n");
            simulateKeyPressFocusWin(display, XK_K);
            simulateKeyPressFocusWin(display, XK_Z);
            simulateKeyPressFocusWin(display, XK_Z);
            sleep(14);
            printf("THORWING POKEBALL DONE\n");


            int capturedScreenshot = captureScreen(display, 1920 + POSX, 906, WIDTH, 130, "capturedScreenshot.ppm");
            sleep(1);

            if (screenshotSuccess != 0) {
                return 1;
                break;
            }

            sleep(1);
            char* capturedText = readImageText("capturedScreenshot.ppm");
            if (capturedText == NULL) {
                return 1;
                break;
            }

            while (strstr(capturedText, "Ditto atrapado!") == NULL) {

                printf("POKEBALL FAILED\n");
                printf("TRYING AGAIN POKEBALL\n");
                simulateKeyPressFocusWin(display, XK_K);
                simulateKeyPressFocusWin(display, XK_Z);
                simulateKeyPressFocusWin(display, XK_Z);
                printf("POKEBALL WAS THREW\n");

                capturedScreenshot = captureScreen(display, 1920 + POSX, 906, WIDTH, 130, "capturedScreenshot.ppm");
                sleep(1);

                if (screenshotSuccess != 0) {
                    return 1;
                    break;
                }

                sleep(1);
                capturedText = readImageText("capturedScreenshot.ppm");
                if (capturedText == NULL) {
                    return 1;
                    break;
                }
                sleep(5);

            }
            

            free(text);
            free(capturedText);
            free(fightText);

        } else {
          printf("SKIPPING THIS TURN\n");
          simulateKeyPressFocusWinDelay(1000000, display, XK_H);
          simulateKeyPressFocusWinDelay(1000000, display, XK_K);
        }   

        sleep(2);
    }

    XCloseDisplay(display);

    return 0;
}
