#include "achievement_driver.hpp"

#include <algorithm>
#include <cstring>

namespace host {

AchievementDriver::AchievementDriver(const std::string& settingsFile) : file_(settingsFile), progress_(achievements::loadProgress(settingsFile)), watch_(progress_.unlocked) {}

void AchievementDriver::update(long long nowMs)
{
  if(const std::uint32_t fresh = watch_.takeNew()) {
    const std::string today = achievements::todayText();
    for(int i = 0; i < achievements::kCount; ++i)
      if((fresh >> i & 1u) && !progress_.has(i)) {
        progress_.unlocked |= 1u << i;
        progress_.dates[static_cast<std::size_t>(i)] = today;
        achievements::saveUnlock(file_, i, today);
        queue_.push_back(i);
      }
  }
  const long long duration = queue_.empty() ? 4500 : 2500;   // when several come together each stays a little shorter
  if(current_ >= 0 && nowMs - shownAt_ >= duration) current_ = -1;
  if(current_ < 0 && !queue_.empty()) { current_ = queue_.front(); queue_.pop_front(); shownAt_ = nowMs; }
}

void AchievementDriver::draw(SDL_Renderer* renderer, long long nowMs)
{
  (void)nowMs;
  if(current_ >= 0) drawAchievementNotice(renderer, achievements::definition(current_).title);
}

void drawAchievementNotice(SDL_Renderer* renderer, const char* title)
{
  const float charSize = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
  int w = 0, h = 0;
  SDL_GetRenderOutputSize(renderer, &w, &h);
  float scale = std::max(1.0f, std::floor(static_cast<float>(h) / 480.0f));
  const float columns = static_cast<float>(std::max<std::size_t>(std::strlen(title), 22) + 4);
  while(scale > 1.0f && columns * charSize * scale > static_cast<float>(w) * 0.95f) scale -= 1.0f;
  SDL_SetRenderScale(renderer, scale, scale);
  const float boxW = columns * charSize, boxH = 4.0f * charSize;
  const float x0 = (static_cast<float>(w) / scale - boxW) / 2, y0 = charSize;
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 235);
  const SDL_FRect box{x0, y0, boxW, boxH};
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 255, 210, 0, 255);   // Pac-Man yellow
  SDL_RenderRect(renderer, &box);
  SDL_RenderDebugText(renderer, x0 + 2 * charSize, y0 + charSize * 0.75f, "ACHIEVEMENT UNLOCKED");
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderDebugText(renderer, x0 + 2 * charSize, y0 + charSize * 2.25f, title);
  SDL_SetRenderScale(renderer, 1.0f, 1.0f);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

}  // namespace host
