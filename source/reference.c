#include <citro2d.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define BTN_CLICKED 1
#define BTN_NOT_CLICKED 0

#define BTN_SELECTED 1
#define BTN_NOT_SELECTED 0

#define BTNCOUNT 2

struct GuiButton{
	int rectX;
	int rectY;
	int rectW;
	int rectH;
	int selected;
	int clicked;
	int go;
	char* text[255];
};

void drawButton(struct GuiButton button) {
	int rectZ = 0;
	u32 clrBlue  = C2D_Color32(0x00, 0x00, 0xFF, 0xFF);
	u32 blurple = C2D_Color32(0x72, 0x89, 0xDA, 0xFF);
	u32 clrWhite = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
	u32 clrRed   = C2D_Color32(0xFF, 0x00, 0x00, 0xFF);

	if(button.clicked) {
		C2D_DrawLine(button.rectX - 1, button.rectY - 1, clrBlue, button.rectX - 1, button.rectY + button.rectH + 1, clrBlue, 4, 1);
		C2D_DrawLine(button.rectX - 2, button.rectY, clrBlue, button.rectX + button.rectW + 2, button.rectY, clrBlue, 4, 1);
		C2D_DrawLine(button.rectX - 2, button.rectY + button.rectH, clrBlue, button.rectX + button.rectW + 2, button.rectY + button.rectH, clrBlue, 4, 1);
		C2D_DrawLine(button.rectX + button.rectW + 1, button.rectY - 1, clrBlue, button.rectX + button.rectW + 1, button.rectY + button.rectH + 1, clrBlue, 4, 1);
	} else if(button.selected) {
		C2D_DrawLine(button.rectX - 2, button.rectY - 1, clrBlue, button.rectX - 2, button.rectY + button.rectH + 1, clrBlue, 4, 1);
		C2D_DrawLine(button.rectX - 2, button.rectY - 1, clrBlue, button.rectX + button.rectW + 2, button.rectY - 1, clrBlue, 4, 1);
		C2D_DrawLine(button.rectX - 2, button.rectY + button.rectH + 1, clrBlue, button.rectX + button.rectW + 2, button.rectY + button.rectH + 1, clrBlue, 4, 1);
		C2D_DrawLine(button.rectX + button.rectW + 2, button.rectY - 1, clrBlue, button.rectX + button.rectW + 2, button.rectY + button.rectH + 1, clrBlue, 4, 1);
	}
	C2D_DrawRectSolid(button.rectX, button.rectY, rectZ, button.rectW, button.rectH, blurple);

	// display text
	C2D_TextBuf myTextBuf = C2D_TextBufNew(15);
	C2D_Text myText;

	C2D_TextParse(&myText, myTextBuf, *button.text);
	
	C2D_TextOptimize(&myText);

	float outWidth = 0;
	float outHeight = 0;
	C2D_TextGetDimensions(&myText, 0.8, 0.8, &outWidth, &outHeight);
	C2D_DrawText(&myText, C2D_WithColor | C2D_AlignCenter, button.rectX + (button.rectW / 2), button.rectY + (button.rectH / 2) - (outHeight / 2), 1, 0.8, 0.8, clrWhite);
}

void clearSelectedButtons(struct GuiButton *screenButtons, int count) {
	for(int y = 0; y < count; y++) {
		screenButtons[y].selected = BTN_NOT_SELECTED;
	}
}

void clearClickedButtons(struct GuiButton *screenButtons, int count) {
	for(int y = 0; y < count; y++) {
		screenButtons[y].clicked = BTN_NOT_CLICKED;
	}
}

int getSelectedButton(struct GuiButton *screenButtons, int count) {
	int selectedBtn = -1;
	for(int y = 0; y < count; y++) {
		if(screenButtons[y].selected == BTN_SELECTED) {
			selectedBtn = y;
			break;
		}
	}
	return selectedBtn;
}

void goPreviousButton(struct GuiButton *screenButtons, int count, int jump) {
	int selectedBtn = getSelectedButton(screenButtons, count);
	selectedBtn-= jump;
	if(selectedBtn < 1) {
		selectedBtn = 0;
	}

	clearSelectedButtons(screenButtons, count);
	screenButtons[selectedBtn].selected = BTN_SELECTED;
}

void goNextButton(struct GuiButton *screenButtons, int count, int jump) {
	int selectedBtn = getSelectedButton(screenButtons, count);
	if(selectedBtn == -1) {
		selectedBtn = 0;
	} else {
		selectedBtn+= jump;
	}

	if(selectedBtn > count - 1) {
		selectedBtn = count - 1;
	}

	clearSelectedButtons(screenButtons, count);
	screenButtons[selectedBtn].selected = BTN_SELECTED;
}

void drawButtons(struct GuiButton *screenButtons, int count) {

	u32 kDown = hidKeysHeld();

	for(int i = 0; i < count; i++) {

		// touchscreen check
		if (kDown & KEY_TOUCH) {
			touchPosition screenPos;
			hidTouchRead(&screenPos);
			if((screenPos.px > screenButtons[i].rectX && screenPos.px < screenButtons[i].rectX + screenButtons[i].rectW) &&
			(screenPos.py > screenButtons[i].rectY && screenPos.py < screenButtons[i].rectY + screenButtons[i].rectH)) {

				printf("\x1b[11;1HTouch button: %d    ", i + 1);

				clearSelectedButtons(screenButtons, count);
				clearClickedButtons(screenButtons, count);

				screenButtons[i].selected = BTN_SELECTED;
				screenButtons[i].clicked = BTN_CLICKED;
				
			} else {
				clearClickedButtons(screenButtons, count);
			}

			printf("\x1b[13;1HTouch X: %d  ", screenPos.px);
			printf("\x1b[14;1HTouch Y: %d  ", screenPos.py);
			printf("\x1b[15;1HScreen touch: yes");
		} else {
			printf("\x1b[11;1HTouch button: none");
			printf("\x1b[15;1HScreen touch: no ");
			clearClickedButtons(screenButtons, count);
		}

		drawButton(screenButtons[i]);
	}
}

//---------------------------------------------------------------------------------
int NOTmain(int argc, char* argv[]) {
//---------------------------------------------------------------------------------
	// Init libs
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	consoleInit(GFX_TOP, NULL);

	// Create screens
	C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	// Create colors
	u32 blurple = C2D_Color32(0x72, 0x89, 0xDA, 0xFF);
	u32 clrred = C2D_Color32(0xFF, 0x00, 0x00, 0xFF);
	u32 clrgreen = C2D_Color32(0x00, 0xFF, 0x00, 0xFF);
	u32 clrblue = C2D_Color32(0x00, 0x00, 0xFF, 0xFF);

	u32 clrClear = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);

	touchPosition olderScreenPos, oldScreenPos, screenPos;
	int currentTouch = 0, currentPad = 0, difPosX = 0, difPosY = 0, toGainPosX = 0, toGainPosY = 0;
	int yBuf = 0, goMove = 0;

	int totalScreenHeight = 600, finalYpos = 0;

	olderScreenPos.px = BOTTOM_SCREEN_WIDTH / 2;
	olderScreenPos.py = BOTTOM_SCREEN_HEIGHT / 2;
	oldScreenPos.px = BOTTOM_SCREEN_WIDTH / 2;
	oldScreenPos.py = BOTTOM_SCREEN_HEIGHT / 2;
	screenPos.px = BOTTOM_SCREEN_WIDTH / 2;
	screenPos.py = BOTTOM_SCREEN_HEIGHT / 2;
	
	// create buttons
	char text1[] = "first menu";
	char text2[] = "2nd menu";
	char text3[] = "3rd menu";
	char text4[] = "...";
	char textCredits[] = "Credits";
	char textExit[] = "Exit";

	// struct GuiButton screenButtons[BTNCOUNT] = {
	// 	{4, finalYpos + 2, BOTTOM_SCREEN_WIDTH - 18, 26, 1, 0, 0, {text1}},
	// 	{4, finalYpos + 32, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text2}},
	// 	{4, finalYpos + 62, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text3}},
	// 	{4, finalYpos + 92, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos + 122, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos + 152, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos + 182, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos + 212, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos + 242, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos + 272, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos + 302, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {textCredits}},
	// 	{4, finalYpos + 332, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {textExit}}
	// };

	// struct GuiButton screenButtons[BTNCOUNT] = {
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 1, 0, 0, {text1}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text2}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text3}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text4}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {textCredits}},
	// 	{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {textExit}}
	// };

	struct GuiButton screenButtons[BTNCOUNT] = {
		{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 1, 0, 0, {text1}},
		{4, finalYpos, BOTTOM_SCREEN_WIDTH - 18, 26, 0, 0, 0, {text2}},
	};

	// Main loop
	while (aptMainLoop())
	{

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(bottom, clrClear);
		C2D_SceneBegin(bottom);

		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysHeld();
		if(kDown & KEY_START)
			break; // break in order to return to hbmenu

		if (kDown & KEY_TOUCH) {
			// This is for scrolling
			olderScreenPos = oldScreenPos;
			oldScreenPos = screenPos;

			hidTouchRead(&screenPos);

			// This is scrolling support
			if(currentTouch == 0) {
				currentTouch = 1;
				yBuf = yBuf - (difPosY - toGainPosY);
				olderScreenPos = screenPos;
				oldScreenPos = screenPos;
			}

			difPosX = olderScreenPos.px - screenPos.px;
			difPosY = olderScreenPos.py - screenPos.py;
			yBuf+= screenPos.py - oldScreenPos.py;

			finalYpos = yBuf;
			if(finalYpos < BOTTOM_SCREEN_HEIGHT - totalScreenHeight) {
				finalYpos = BOTTOM_SCREEN_HEIGHT - totalScreenHeight;
				yBuf = finalYpos;
			} else if(finalYpos > 0) {
				finalYpos = 0;
				yBuf = 0;
			}
		} else {
			if(currentTouch == 1) {
				currentTouch = 0;
				difPosX*=1.5;
				difPosY*=1.5;
				toGainPosX = difPosX;
				toGainPosY = difPosY;
			} else {
				toGainPosX*= 0.8;
				toGainPosY*= 0.8;
			}

			// buttons
			if((kDown & KEY_DOWN) || (kDown & KEY_UP) || (kDown & KEY_LEFT) || (kDown & KEY_RIGHT)) {
				if(currentPad == 0) {
					currentPad = 1;
					difPosX = 0;
					difPosY = 0;
					toGainPosX = 0;
					toGainPosY = 0;
					goMove = 7;
				}
				goMove++;
			} else {
				currentPad = 0;
			}

			if(kDown & KEY_DOWN) {
				if(goMove == 8) {
					finalYpos-= 50;
					if(finalYpos < BOTTOM_SCREEN_HEIGHT - totalScreenHeight) {
						finalYpos = BOTTOM_SCREEN_HEIGHT - totalScreenHeight;
					}
					yBuf = finalYpos;

					goNextButton(screenButtons, BTNCOUNT, 1);
					goMove = 0;
				}
			}

			if(kDown & KEY_UP) {
				if(goMove == 8) {
					finalYpos+= 50;
					if(finalYpos > 0) {
						finalYpos = 0;
					}
					yBuf = finalYpos;

					goPreviousButton(screenButtons, BTNCOUNT, 1);
					goMove = 0;
				}
			}

			if(kDown & KEY_RIGHT) {
				if(goMove == 8) {
					finalYpos-= 50 * 5;
					if(finalYpos < BOTTOM_SCREEN_HEIGHT - totalScreenHeight) {
						finalYpos = BOTTOM_SCREEN_HEIGHT - totalScreenHeight;
					}
					yBuf = finalYpos;

					goNextButton(screenButtons, BTNCOUNT, 5);
					goMove = 0;
				}
			}

			if(kDown & KEY_LEFT) {
				if(goMove == 8) {
					finalYpos+= 50 * 5;
					if(finalYpos > 0) {
						finalYpos = 0;
					}
					yBuf = finalYpos;

					goPreviousButton(screenButtons, BTNCOUNT, 5);
					goMove = 0;
				}
			}
			// end buttons

			if(!((kDown & KEY_DOWN) || (kDown & KEY_UP))) {
				finalYpos = yBuf - (difPosY - toGainPosY);
				if(finalYpos < BOTTOM_SCREEN_HEIGHT - totalScreenHeight) {
					finalYpos = BOTTOM_SCREEN_HEIGHT - totalScreenHeight;
					yBuf = finalYpos;
				} else if(finalYpos > 0) {
					finalYpos = 0;
					yBuf = 0;
				}
			}
		}

		for(int z = 0; z < BTNCOUNT; z++) {
			screenButtons[z].rectY = finalYpos + 2 + (50 * z);
		}
		

		//C2D_DrawEllipseSolid(0, finalYpos, 0, BOTTOM_SCREEN_WIDTH, 280, blurple);
		/* C2D_DrawRectSolid(4, finalYpos + 2, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrred);
		C2D_DrawRectSolid(4, finalYpos + 52, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrgreen);
		C2D_DrawRectSolid(4, finalYpos + 102, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrblue);
		C2D_DrawRectSolid(4, finalYpos + 152, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrred);
		C2D_DrawRectSolid(4, finalYpos + 202, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrgreen);
		C2D_DrawRectSolid(4, finalYpos + 252, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrblue);
		C2D_DrawRectSolid(4, finalYpos + 302, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrred);
		C2D_DrawRectSolid(4, finalYpos + 352, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrgreen);
		C2D_DrawRectSolid(4, finalYpos + 402, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrblue);
		C2D_DrawRectSolid(4, finalYpos + 452, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrred);
		C2D_DrawRectSolid(4, finalYpos + 502, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrgreen);
		C2D_DrawRectSolid(4, finalYpos + 552, 0, BOTTOM_SCREEN_WIDTH - 18, 46, clrblue); */
		drawButtons(screenButtons, BTNCOUNT);

		// scrollbar
		if(totalScreenHeight <= BOTTOM_SCREEN_HEIGHT) {
			C2D_DrawRectSolid(BOTTOM_SCREEN_WIDTH - 10, 0, 0, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT, blurple);
		} else {
			C2D_DrawRectSolid(BOTTOM_SCREEN_WIDTH - 10, (-finalYpos) * (BOTTOM_SCREEN_HEIGHT - 50) / -(BOTTOM_SCREEN_HEIGHT - totalScreenHeight), 0, BOTTOM_SCREEN_WIDTH, 50, blurple);
		}


		printf("\x1b[1;1HOld px: %d | Old py: %d    ", olderScreenPos.px, olderScreenPos.py);
		printf("\x1b[2;1HNew px: %d | New py: %d    ", screenPos.px, screenPos.py);
		printf("\x1b[4;1HDif px: %d | Dif py: %d    ", difPosX, difPosY);

		printf("\x1b[6;1HyBuf: %d      ", yBuf);
		printf("\x1b[7;1HfinalYpos: %d      ", finalYpos);

		C3D_FrameEnd(0);

		if((kDown & KEY_A) && screenButtons[10].selected) {
			while(1) {
				C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
				C2D_TargetClear(bottom, blurple);
				C2D_SceneBegin(bottom);

				hidScanInput();

				// display text
				C2D_TextBuf myTextBuf = C2D_TextBufNew(15);
				C2D_Text myText;

				C2D_TextParse(&myText, myTextBuf, "Credits page");
				
				C2D_TextOptimize(&myText);

				float outWidth = 0;
				float outHeight = 0;
				C2D_TextGetDimensions(&myText, 1, 1, &outWidth, &outHeight);
				C2D_DrawText(&myText, C2D_WithColor | C2D_AlignCenter, BOTTOM_SCREEN_WIDTH / 2, (BOTTOM_SCREEN_HEIGHT / 2) - (outHeight / 2), 1, 1, 1, clrClear);

				C3D_FrameEnd(0);

				// Respond to user input
				u32 kDown = hidKeysHeld();
				if(kDown & KEY_B)
					break; // break in order to return to main loop
			}
		}

		if((kDown & KEY_A) && screenButtons[11].selected) {
			break;
		}
	}

	// Deinit libs
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}
