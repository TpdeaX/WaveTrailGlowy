#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include "CustomSettings.hpp"

namespace Utils {
	GLubyte convertOpacitySimplf(float opaTM) { return static_cast<GLubyte>(255 * opaTM); }
	cocos2d::ccColor3B shiftHue(cocos2d::ccColor3B& color, int shift, int max, int min) {
		if (color.r == max && color.g != max && color.b == min) {
			color.g = std::min(max, color.g + shift);
		}
		else if (color.r != min && color.g == max && color.b == min) {
			color.r = std::max(min, color.r - shift);
		}
		else if (color.r == min && color.g == max && color.b != max) {
			color.b = std::min(max, color.b + shift);
		}
		else if (color.r == min && color.g != min && color.b == max) {
			color.g = std::max(min, color.g - shift);
		}
		else if (color.r != max && color.g == min && color.b == max) {
			color.r = std::min(max, color.r + shift);
		}
		else if (color.r == max && color.g == min && color.b != min) {
			color.b = std::max(min, color.b - shift);
		}
		else {

			color.r = std::max(min, color.r - shift);

			int max_val = std::max(color.g, color.b);
			if (max_val == color.g) {
				color.g = std::min(max, color.g + shift);
			}
			else if (max_val == color.b) {
				color.b = std::min(max, color.b + shift);
			}
		}

		return color;
	}

	struct RGB {
		cocos2d::ccColor3B color = { 0 ,0 , 0 };
		double ratio = 0.0;
		bool increasing = true;
	};

	cocos2d::ccColor3B interpolateColors(RGB& color1, RGB& color2, double shift) {
		cocos2d::ccColor3B result;
		result.r = color1.color.r + static_cast<int>((color2.color.r - color1.color.r) * color1.ratio);
		result.g = color1.color.g + static_cast<int>((color2.color.g - color1.color.g) * color1.ratio);
		result.b = color1.color.b + static_cast<int>((color2.color.b - color1.color.b) * color1.ratio);

		if (color1.increasing) {
			color1.ratio += shift;
			if (color1.ratio >= 1.0)
				color1.increasing = false;
		}
		else {
			color1.ratio -= shift;
			if (color1.ratio <= 0.0)
				color1.increasing = true;
		}

		return result;
	}

	template <class R, class T>
	R& from(T base, intptr_t offset) {
		return *reinterpret_cast<R*>(reinterpret_cast<uintptr_t>(base) + offset);
	}
}

class WaveTrailGlowy : public cocos2d::CCLayer {
public:
	static WaveTrailGlowy* create(PlayLayer* layer, PlayerObject* player);
	virtual bool init();
	void update(float delta);
	void onUpdateTrail();
	std::vector<cocos2d::CCNode*> getPartsToRemove();
	void onDisappearance(float duration, std::function<void()> callback);
	void addNewPartGlowy(cocos2d::CCPoint pos, cocos2d::ccColor3B color, float scale);
	void resetTrail();
	void setPlayer(PlayerObject* player);
	void setOpacityGlowy(GLubyte opacity);

	GameObject* m_pBatchNode = nullptr;
	PlayerObject* m_pPlayer = nullptr;
	PlayLayer* m_playLayer = nullptr;
	cocos2d::CCPoint m_lastPositionAdded = {};

	cocos2d::ccColor3B rainbowColor = { 255, 0, 0 };
	cocos2d::ccColor3B rainbowPastelColor = { 245, 155, 155 };

	Utils::RGB fadeInitialColor;
	Utils::RGB fadeFinalColor;

	cocos2d::ccColor3B fadecurrentColor = { 245, 155, 155 };

	std::vector<cocos2d::CCSprite*> m_parts = {};
	std::vector<cocos2d::CCPoint> m_positions = {};

	float m_pOpacitySpritesOriginal = Utils::convertOpacitySimplf(0.5f);
	float m_pOpacitySpriteCurrent = Utils::convertOpacitySimplf(0.5f);

	bool m_pOnHide = false;
	bool m_hideOnDead = true;
	bool prevDartState = false;

	float timer = 0.0f;
	const float interval = 0.02f;
	bool m_playerIndex = 0;
};

WaveTrailGlowy* WaveTrailGlowy::create(PlayLayer* layer, PlayerObject* player)
{
	auto ret = new WaveTrailGlowy();
	if (ret && ret->init())
	{
		ret->autorelease();
		ret->setPlayer(player);
		ret->scheduleUpdate();
		ret->m_playLayer = layer;
		return ret;
	}
	CC_SAFE_RELEASE(ret);
	return nullptr;
}


bool WaveTrailGlowy::init() {
	if (!cocos2d::CCLayer::init()) {
		return false;
	}
	m_pBatchNode = GameObject::createWithFrame("emptyFrame.png");
	


	addChild(m_pBatchNode);
	return true;
}

void WaveTrailGlowy::update(float delta) {
	this->m_playLayer = PlayLayer::get();
	if (!m_playLayer || !m_pPlayer) return;

	if (m_playLayer && m_playLayer->m_player1 && m_pPlayer) {
		m_playerIndex = (m_pPlayer == m_playLayer->m_player1) ? 0 : 1;
	}

	fadeInitialColor.color = Mod::get()->getSettingValue<cocos2d::ccColor3B>(m_playerIndex ? "fade-color-glowy-p2-initial" : "fade-color-glowy-p1-initial");
	fadeFinalColor.color = Mod::get()->getSettingValue<cocos2d::ccColor3B>(m_playerIndex ? "fade-color-glowy-p2-final" : "fade-color-glowy-p1-final");

	Utils::shiftHue(rainbowColor, static_cast<int>(5 * Mod::get()->getSettingValue<double>(m_playerIndex ? "rainbow-glowy-p2-speed" : "rainbow-glowy-p1-speed")), 255, 0);
	Utils::shiftHue(rainbowPastelColor, static_cast<int>(5 * Mod::get()->getSettingValue<double>(m_playerIndex ? "rainbow-pastel-glowy-p2-speed" : "rainbow-pastel-glowy-p1-speed")), 245, 155);
	fadecurrentColor = Utils::interpolateColors(fadeInitialColor, fadeFinalColor, 0.01 * Mod::get()->getSettingValue<double>(m_playerIndex ? "fade-color-glowy-p2-speed" : "fade-color-glowy-p1-speed"));

	cocos2d::CCPoint currentPosition = m_pPlayer->getPosition();
	float distanceMoved = currentPosition.getDistance(m_lastPositionAdded);

	int glowsToAdd = std::max(static_cast<int>(distanceMoved / 0.5f) - static_cast<int>(m_parts.size()), 0);



	for (int i = 0; i < glowsToAdd; ++i) {

		bool currDartState = m_pPlayer->m_isDart;
		if ((!prevDartState && currDartState) || m_pPlayer->m_isHidden || !m_pPlayer) {
			resetTrail();
		}
		else {
			if ((prevDartState && !currDartState) || Utils::from<bool>(m_playLayer, 0x2c28) ||
				(m_pPlayer == m_playLayer->m_player2 && !Utils::from<bool>(m_playLayer, 0x36e)) ||
				(Utils::from<bool>(m_playLayer, 0x2ac2) && m_hideOnDead) || m_pOnHide ||
				Utils::from<CCArray*>(m_pPlayer->m_waveTrail, 0x158)->count() == 0) {
				this->onDisappearance(0.5f, [&] { m_pOnHide = false; });
			}
			else {
				this->onUpdateTrail();
			}
		}
		prevDartState = currDartState;
	}
}



void WaveTrailGlowy::onUpdateTrail() {
	auto colorWave = reinterpret_cast<cocos2d::CCNodeRGBA*>(m_pPlayer->m_waveTrail)->getColor();
	bool updateColorAllParts = Mod::get()->getSettingValue<bool>(m_playerIndex ? "const-effect-color-p2" : "const-effect-color-p1");

	bool useCustomColorTrail = Mod::get()->getSettingValue<bool>(m_playerIndex ? "custom-color-trail-p2" : "custom-color-trail-p1");

	bool rainbowGlowy = Mod::get()->getSettingValue<bool>(m_playerIndex ? "rainbow-glowy-p2" : "rainbow-glowy-p1");
	bool rainbowPastelGlowy = Mod::get()->getSettingValue<bool>(m_playerIndex ? "rainbow-pastel-glowy-p2" : "rainbow-pastel-glowy-p1");
	bool fadeColorGlowy = Mod::get()->getSettingValue<bool>(m_playerIndex ? "fade-color-glowy-p2" : "fade-color-glowy-p1");


	if (rainbowGlowy || rainbowPastelGlowy) {
		colorWave = rainbowGlowy ? rainbowColor : rainbowPastelColor;
	}
	else if (fadeColorGlowy) {
		colorWave = fadecurrentColor;
	}
	else if (useCustomColorTrail) {
		colorWave = Mod::get()->getSettingValue<cocos2d::ccColor3B>(m_playerIndex ? "color-trail-p2" : "color-trail-p1");
	}

	float smoothed_pulse_size = Utils::from<float>(m_pPlayer->m_waveTrail, 0x168) *
		(Utils::from<float>(m_pPlayer->m_waveTrail, 0x168) > 1.2f ? 0.6f :
			(Utils::from<float>(m_pPlayer->m_waveTrail, 0x168) > 0.5f) ? 1.f : 2.f) * 1.f;

	if (Mod::get()->getSettingValue<bool>("const-size"))
		smoothed_pulse_size = Mod::get()->getSettingValue<double>("size-glowy-wave-trail");

	auto newPos = cocos2d::CCPoint(m_pPlayer->getPositionX(), m_pPlayer->getPositionY());
	std::vector<cocos2d::CCNode*> partsToRemove = this->getPartsToRemove();

	for (auto& partGlowy : m_parts) {
		if (updateColorAllParts)
			partGlowy->setChildColor(colorWave);
		partGlowy->setScale(smoothed_pulse_size);
		partGlowy->setChildOpacity(m_pOpacitySpriteCurrent);
	}

	for (auto& partGlowyToRemove : partsToRemove) {
		m_pBatchNode->removeChild(partGlowyToRemove, true);
		m_parts.erase(std::remove(m_parts.begin(), m_parts.end(), partGlowyToRemove), m_parts.end());
		auto it = std::find_if(m_positions.begin(), m_positions.end(), [&](const cocos2d::CCPoint& pos) {
			return pos.x == partGlowyToRemove->getPosition().x && pos.y == partGlowyToRemove->getPosition().y;
			});
		if (it != m_positions.end())
			m_positions.erase(it);
	}

	if (newPos != m_lastPositionAdded)
		this->addNewPartGlowy(newPos, colorWave, smoothed_pulse_size);
}
std::vector<cocos2d::CCNode*> WaveTrailGlowy::getPartsToRemove() {
	std::vector<cocos2d::CCNode*> ret = {};

	for (auto& partGlowy : m_parts) {
		auto pos = partGlowy->getPosition();
		if (((pos.x - 40.f) - (Utils::from<CCPoint>(m_playLayer, 0x404).x)) < -80.f) {
			ret.push_back(partGlowy);
		}
	}

	return ret;
}

void WaveTrailGlowy::onDisappearance(float duration, std::function<void()> callback) {
	m_pOnHide = true;

	auto onComplete = [this, callback]() {
		resetTrail();
		if (callback) {
			callback();
		}
		};

	if (m_pOpacitySpriteCurrent <= 0) {
		onComplete();
	}
	else {
		m_pOpacitySpriteCurrent -= 5.f / duration;
		m_pOpacitySpriteCurrent = std::max(0.f, m_pOpacitySpriteCurrent);

		for (auto& partGlowy : m_parts) {
			partGlowy->setChildOpacity(m_pOpacitySpriteCurrent);
		}
	}
}

void WaveTrailGlowy::addNewPartGlowy(cocos2d::CCPoint pos, cocos2d::ccColor3B color, float scale) {

	auto partGlowy = cocos2d::CCSprite::createWithSpriteFrameName("emptyFrame.png");
	auto partGlowyTL = cocos2d::CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");
	auto partGlowyTR = cocos2d::CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");
	auto partGlowyBL = cocos2d::CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");
	auto partGlowyBR = cocos2d::CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");

	partGlowyTR->setPositionX(30.f);
	partGlowyTR->setFlipX(true);
	partGlowyBL->setPosition({ 30.f, -30.f });
	partGlowyBL->setFlipX(true);
	partGlowyBL->setFlipY(true);
	partGlowyBR->setPositionY(-30.f);
	partGlowyBR->setFlipY(true);
	partGlowyTL->setAnchorPoint({ 1.f, 0.f });
	partGlowyTR->setAnchorPoint({ 1.f, 0.f });
	partGlowyBL->setAnchorPoint({ 1.f, 0.f });
	partGlowyBR->setAnchorPoint({ 1.f, 0.f });

	partGlowyTL->setBlendFunc(cocos2d::ccBlendFunc{ GL_ONE, GL_ONE_MINUS_SRC_ALPHA });
	partGlowyTR->setBlendFunc(cocos2d::ccBlendFunc{ GL_ONE, GL_ONE_MINUS_SRC_ALPHA });
	partGlowyBL->setBlendFunc(cocos2d::ccBlendFunc{ GL_ONE, GL_ONE_MINUS_SRC_ALPHA });
	partGlowyBR->setBlendFunc(cocos2d::ccBlendFunc{ GL_ONE, GL_ONE_MINUS_SRC_ALPHA });

	partGlowy->addChild(partGlowyTL);
	partGlowy->addChild(partGlowyTR);
	partGlowy->addChild(partGlowyBL);
	partGlowy->addChild(partGlowyBR);

	partGlowy->setPosition(pos);
	partGlowy->setChildColor(color);
	partGlowy->setChildOpacity(m_pOpacitySpriteCurrent);
	partGlowy->setScale(scale);

	m_pBatchNode->addChild(partGlowy);
	m_parts.push_back(partGlowy);
	m_lastPositionAdded = pos;
}

void WaveTrailGlowy::resetTrail() {
	m_pBatchNode->removeAllChildrenWithCleanup(true);
	m_parts.clear();
	m_positions.clear();
	m_pOpacitySpriteCurrent = m_pOpacitySpritesOriginal;
	m_pOnHide = false;
}

void WaveTrailGlowy::setPlayer(PlayerObject* player) {
	m_pPlayer = player;
}

void WaveTrailGlowy::setOpacityGlowy(GLubyte opacity) {
	if (m_pOnHide) {
		m_pOpacitySpritesOriginal = opacity;
		return;
	}
	m_pOpacitySpritesOriginal = m_pOpacitySpriteCurrent = opacity;
}

class $modify(PlayLayer) {

	WaveTrailGlowy* getWaveTrailGlowy(bool player1) {
		return typeinfo_cast<WaveTrailGlowy*>(this->m_objectLayer->getChildByID(player1 ? "WaveTrailGlowy_m_player1" : "WaveTrailGlowy_m_player2"));
	}

	bool init(GJGameLevel * level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
			return false;
		}

		if (Mod::get()->getSettingValue<bool>("glowy-enabled")) {
			auto glowWaveTrailP1 = WaveTrailGlowy::create(this, this->m_player1);
			auto glowWaveTrailP2 = WaveTrailGlowy::create(this, this->m_player2);
			if (glowWaveTrailP1 && glowWaveTrailP2) {
				glowWaveTrailP1->setID("WaveTrailGlowy_m_player1");
				glowWaveTrailP2->setID("WaveTrailGlowy_m_player2");
				glowWaveTrailP1->setOpacityGlowy(Utils::convertOpacitySimplf(Mod::get()->getSettingValue<double>("opacity-glowy-wave-trail")));
				glowWaveTrailP2->setOpacityGlowy(Utils::convertOpacitySimplf(Mod::get()->getSettingValue<double>("opacity-glowy-wave-trail")));
				glowWaveTrailP1->setVisible(true);
				glowWaveTrailP2->setVisible(true);
				this->m_objectLayer->addChild(glowWaveTrailP1, -4);
				this->m_objectLayer->addChild(glowWaveTrailP2, -4);
			}
		}

		return true;
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		auto wtp1 = getWaveTrailGlowy(true);
		auto wtp2 = getWaveTrailGlowy(false);
		if (wtp1) {
			wtp1->resetTrail();
		}
		if (wtp2) {
			wtp2->resetTrail();
		}
	}

};


class $modify(PlayerObject) {

	WaveTrailGlowy* getWaveTrailGlowy(bool player1) {

		if (!PlayLayer::get()) {
			return nullptr;
		}

		return typeinfo_cast<WaveTrailGlowy*>(PlayLayer::get()->m_objectLayer->getChildByID(player1 ? "WaveTrailGlowy_m_player1" : "WaveTrailGlowy_m_player2"));
	}

	TodoReturn resetStreak() {
		PlayerObject::resetStreak();
		auto pl = PlayLayer::get();
		if (pl) {
			if (PlayLayer::get()->m_player1 == this || PlayLayer::get()->m_player2 == this) {
				auto glowWave = dynamic_cast<WaveTrailGlowy*>(getWaveTrailGlowy(PlayLayer::get()->m_player1 == this));
				if (glowWave) {
					glowWave->resetTrail();
				}
			}
		}
	}
};

$on_mod(Loaded) {
	Mod::get()->addCustomSetting<SettingSectionValue>("title-seccion-p1-custom", "none");
	Mod::get()->addCustomSetting<SettingSectionValue>("title-seccion-p2-custom", "none");
}