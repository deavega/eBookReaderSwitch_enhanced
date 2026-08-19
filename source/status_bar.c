#include <time.h>
#include <switch.h>

#include "common.h"
#include "SDL_helper.h"
#include "status_bar.h"
#include "textures.h"

static char *Clock_GetCurrentTime(void) {
	static char buffer[10];

	// gmtime() always shows raw UTC -- it ignores whatever timezone is
	// actually configured on the console. This app's timeInitialize() call
	// (in main.cpp) sets up libnx's Time service, so ask it directly for
	// the calendar time under the console's configured region instead.
	int hours = 0, minutes = 0;

	u64 timestamp = 0;
	if (R_SUCCEEDED(timeGetCurrentTime(TimeType_UserSystemClock, &timestamp))) {
		TimeCalendarTime caltime;
		TimeCalendarAdditionalInfo addinfo;
		if (R_SUCCEEDED(timeToCalendarTimeWithMyRule(timestamp, &caltime, &addinfo))) {
			hours = caltime.hour;
			minutes = caltime.minute;
		}
	}

	bool amOrPm = false;
	
	if (hours < 12)
		amOrPm = true;
	if (hours == 0)
		hours = 12;
	else if (hours > 12)
		hours = hours - 12;

	if ((hours >= 1) && (hours < 10))
		snprintf(buffer, 10, "%2i:%02i %s", hours, minutes, amOrPm ? "AM" : "PM");
	else
		snprintf(buffer, 10, "%2i:%02i %s", hours, minutes, amOrPm ? "AM" : "PM");

	return buffer;
}

// Draws the battery icon+percentage so its right edge lands exactly at
// `right_edge` -- callers no longer need to guess its width up front.
static void StatusBar_GetBatteryStatus(int right_edge, int y) {
	u32 percent = 0;
	//ChargerType state;
	PsmChargerType state;
	int width = 0;
	char buf[5];

	if (R_FAILED(psmGetChargerType(&state)))
		state = 0;
	
	if (R_SUCCEEDED(psmGetBatteryChargePercentage(&percent))) {
		SDL_Texture *batteryImage;
		if (percent < 20) {
			//SDL_DrawImage(RENDERER, battery_low, x, 3);
			batteryImage = battery_low;
		} else if ((percent >= 20) && (percent < 30)) {
			if (state != 0) {
				//SDL_DrawImage(RENDERER, battery_20_charging, x, 3);
				batteryImage = battery_20_charging;
			} else {
				//SDL_DrawImage(RENDERER, battery_20, x, 3);
				batteryImage = battery_20;
			}
		} else if ((percent >= 30) && (percent < 50)) {
			if (state != 0) {
				//SDL_DrawImage(RENDERER, battery_50_charging, x, 3);
				batteryImage = battery_50_charging;
			} else {
				//SDL_DrawImage(RENDERER, battery_50, x, 3);
				batteryImage = battery_50;
			}
		} else if ((percent >= 50) && (percent < 60)) {
			if (state != 0) {
				//SDL_DrawImage(RENDERER, battery_50_charging, x, 3);
				batteryImage = battery_50_charging;
			} else {
				//SDL_DrawImage(RENDERER, battery_50, x, 3);
				batteryImage = battery_50;
			}
		} else if ((percent >= 60) && (percent < 80)) {
			if (state != 0) {
				//SDL_DrawImage(RENDERER, battery_60_charging, x, 3);
				batteryImage = battery_60_charging;
			} else {
				//SDL_DrawImage(RENDERER, battery_60, x, 3);
				batteryImage = battery_60;
			}
		} else if ((percent >= 80) && (percent < 90)) {
			if (state != 0) {
				//SDL_DrawImage(RENDERER, battery_80_charging, x, 3);
				batteryImage = battery_80_charging;
			} else {
				//SDL_DrawImage(RENDERER, battery_80, x, 3);
				batteryImage = battery_80;
			}
		} else if ((percent >= 90) && (percent < 100)) {
			if (state != 0) {
				//SDL_DrawImage(RENDERER, battery_90_charging, x, 3);
				batteryImage = battery_90_charging;
			} else {
				//SDL_DrawImage(RENDERER, battery_90, x, 3);
				batteryImage = battery_90;
			}
		} else if (percent == 100) {
			if (state != 0) {
				//SDL_DrawImage(RENDERER, battery_full_charging, x, 3);
				batteryImage = battery_full_charging;
			} else {
				//SDL_DrawImage(RENDERER, battery_full, x, 3);
				batteryImage = battery_full;
			}
		}

		snprintf(buf, 5, "%d%%", percent);
		TTF_SizeText(ROBOTO_20, buf, &width, NULL);
		// Icon (34px) + a -2px gap + the text should end exactly at right_edge.
		int x = right_edge - 34 - (-2) - width;
		SDL_DrawHorizonalAlignedImageText(RENDERER, batteryImage, ROBOTO_20, WHITE, buf, x, y, 34, 34, -2, 0);
		//SDL_DrawText(RENDERER, ROBOTO_20, (x + width + 5), y, WHITE, buf);
	} else {
		snprintf(buf, 5, "%d%%", percent);
		TTF_SizeText(ROBOTO_20, buf, &width, NULL);
		int x = right_edge - 34 - (-2) - width;
		SDL_DrawHorizonalAlignedImageText(RENDERER, battery_unknown, ROBOTO_20, WHITE, buf, x, y, 34, 34, -2, 0);
		/*SDL_DrawText(RENDERER, ROBOTO_20, (x + width + 5), y, WHITE, buf);
		SDL_DrawImage(RENDERER, battery_unknown, x, 1);*/
	}
}

void StatusBar_DisplayTime(bool portriat) {
	int timeWidth = 0, timeHeight = 0;
	TTF_SizeText(ROBOTO_25, Clock_GetCurrentTime(), &timeWidth, &timeHeight);
	int helpWidth, helpHeight;
	TTF_SizeText(ROBOTO_20, "\"+\" - Help", &helpWidth, &helpHeight);

	if (portriat) {
		int timeX = (1280 - timeWidth) + timeHeight;
  	      	int timeY = (720 - timeWidth) + 15;
        	SDL_DrawRotatedText(RENDERER, ROBOTO_25, (double) 90, timeX, timeY, WHITE, Clock_GetCurrentTime());
		
		int helpX = (1280 - helpWidth) + 21;
		int helpY = (720 - helpHeight) - (720 - timeY) - 75;
		SDL_DrawRotatedText(RENDERER, ROBOTO_20, (double) 90, helpX, helpY, WHITE, "\"+\" - Help");
		//SDL_DrawRotatedText(RENDERER, ROBOTO_25, (double) 90, 1270 - width, (720 - height), WHITE, Clock_GetCurrentTime());
	} else {
		// Positioned sequentially from the right edge using each element's
		// real measured width, so nothing can drift into anything else --
		// the previous fixed offsets assumed a battery percentage of a
		// specific digit count and drifted into the help text otherwise.
		const int rightMargin = 1260;
		const int gap = 22;

		int timeX = rightMargin - timeWidth;
		SDL_DrawText(RENDERER, ROBOTO_25, timeX, (40 - timeHeight) / 2, WHITE, Clock_GetCurrentTime());

		int battRightEdge = timeX - gap;
		StatusBar_GetBatteryStatus(battRightEdge, 40);

		// Width of the battery widget (icon + gap + percentage text), so the
		// help text can start after it without needing to know its innards.
		char battBuf[5];
		int battTextWidth = 0;
		u32 battPercent = 0;
		psmGetBatteryChargePercentage(&battPercent);
		snprintf(battBuf, 5, "%d%%", battPercent);
		TTF_SizeText(ROBOTO_20, battBuf, &battTextWidth, NULL);
		int battWidgetWidth = 34 - (-2) + battTextWidth;

		int helpX = battRightEdge - battWidgetWidth - gap - helpWidth;
		SDL_DrawText(RENDERER, ROBOTO_20, helpX, (40 - helpHeight) / 2, WHITE, "\"+\" - Help");
	}
}
