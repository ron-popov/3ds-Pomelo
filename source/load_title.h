#pragma once

#include "utils.h"
#include <3ds.h>

bool loadSMDHContent(FS_Archive exefsArchive, SMDH *p_smdh,
					 titleGame *titleGameOut);

bool loadBannerContent(FS_Archive exefsArchive, titleGame *titleGameOut);

bool loadTitleMetadata(u64 titleId, FS_MediaType mediaType,
					   titleGame *titleGameOut);

bool loadTitlesFromMediaType(FS_MediaType mediaType, u8 maxTitleCount,
							 titleGame **games, u8 *games_counter);

bool loadTitles(titleGame **games, u8 *games_counter);

bool renderTitleBanner(titleGame *titleGame, C3D_RenderTarget *renderTarget);