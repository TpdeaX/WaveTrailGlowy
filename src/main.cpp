#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/binding/HardStreak.hpp>

using namespace geode::prelude;

namespace Utils {
	inline GLubyte convertOpacitySimplf(float opaTM) {
		return static_cast<GLubyte>(std::clamp(opaTM, 0.0f, 1.0f) * 255.f);
	}

	inline cocos2d::ccColor3B shiftHue(cocos2d::ccColor3B& color, int shift, int maxVal, int minVal) {
		if (color.r == maxVal && color.g != maxVal && color.b == minVal) {
			color.g = std::min(maxVal, color.g + shift);
		}
		else if (color.r != minVal && color.g == maxVal && color.b == minVal) {
			color.r = std::max(minVal, color.r - shift);
		}
		else if (color.r == minVal && color.g == maxVal && color.b != maxVal) {
			color.b = std::min(maxVal, color.b + shift);
		}
		else if (color.r == minVal && color.g != minVal && color.b == maxVal) {
			color.g = std::max(minVal, color.g - shift);
		}
		else if (color.r != maxVal && color.g == minVal && color.b == maxVal) {
			color.r = std::min(maxVal, color.r + shift);
		}
		else if (color.r == maxVal && color.g == minVal && color.b != minVal) {
			color.b = std::max(minVal, color.b - shift);
		}
		else {
			color.r = std::max(minVal, color.r - shift);
			int currentMax = std::max(color.g, color.b);
			if (currentMax == color.g) {
				color.g = std::min(maxVal, color.g + shift);
			}
			else {
				color.b = std::min(maxVal, color.b + shift);
			}
		}
		return color;
	}

	struct RGB {
		cocos2d::ccColor3B color = { 0, 0, 0 };
		double ratio = 0.0;
		bool increasing = true;
	};

	inline cocos2d::ccColor3B interpolateColors(RGB& color1, RGB& color2, double shift) {
		cocos2d::ccColor3B result;
		result.r = static_cast<GLubyte>(color1.color.r + (color2.color.r - color1.color.r) * color1.ratio);
		result.g = static_cast<GLubyte>(color1.color.g + (color2.color.g - color1.color.g) * color1.ratio);
		result.b = static_cast<GLubyte>(color1.color.b + (color2.color.b - color1.color.b) * color1.ratio);

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

	inline void applySpriteColor(cocos2d::CCSprite* sprite, cocos2d::ccColor3B color) {
		if (!sprite) return;
		sprite->setColor(color);
		if (auto children = sprite->getChildren()) {
			for (auto child : CCArrayExt<cocos2d::CCNodeRGBA*>(children)) {
				if (child) {
					child->setColor(color);
				}
			}
		}
	}

	inline void applySpriteOpacity(cocos2d::CCSprite* sprite, GLubyte opacity) {
		if (!sprite) return;
		sprite->setOpacity(opacity);
		if (auto children = sprite->getChildren()) {
			for (auto child : CCArrayExt<cocos2d::CCNodeRGBA*>(children)) {
				if (child) {
					child->setOpacity(opacity);
				}
			}
		}
	}

	inline void applySpriteBlend(cocos2d::CCSprite* sprite, cocos2d::ccBlendFunc blend) {
		if (!sprite) return;
		sprite->setBlendFunc(blend);
		if (auto children = sprite->getChildren()) {
			for (auto child : CCArrayExt<cocos2d::CCSprite*>(children)) {
				if (child) {
					child->setBlendFunc(blend);
				}
			}
		}
	}
}

class WaveTrailGlowy : public cocos2d::CCLayer {
public:
	static WaveTrailGlowy* create(PlayLayer* layer, PlayerObject* player);
	bool init() override;
	void update(float delta) override;
	void onUpdateTrail(cocos2d::CCPoint currentPosition, float distanceMoved);
	std::vector<cocos2d::CCNode*> getPartsToRemove();
	void onDisappearance(float delta, std::function<void()> callback = nullptr);
	void addNewPartGlowy(cocos2d::CCPoint pos, cocos2d::ccColor3B color, float scale);
	void resetTrail();
	void setPlayer(PlayerObject* player);
	void setOpacityGlowy(GLubyte opacity);
	void triggerClickPulse();
	void startDisappearance();
	void stopProducingAndDisappear();

	cocos2d::CCNode* m_pContainerNode = nullptr;
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
	bool m_isLevelEnding = false;
	bool prevDartState = false;

	float timer = 0.0f;
	const float interval = 0.02f;
	bool m_playerIndex = 0;
	float m_clickPulse = 0.0f;
};

WaveTrailGlowy* WaveTrailGlowy::create(PlayLayer* layer, PlayerObject* player) {
	auto ret = new WaveTrailGlowy();
	if (ret && ret->init()) {
		ret->autorelease();
		ret->setPlayer(player);
		if (player) {
			ret->m_lastPositionAdded = player->getPosition();
		}
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
	m_pContainerNode = cocos2d::CCNode::create();
	if (m_pContainerNode) {
		addChild(m_pContainerNode);
	}
	return true;
}

void WaveTrailGlowy::update(float delta) {
	this->m_playLayer = PlayLayer::get();
	if (!m_playLayer || !m_pPlayer) return;

	bool disablePractice = Mod::get()->getSettingValue<bool>("disable-practice");
	if (disablePractice && m_playLayer->m_isPracticeMode) {
		if (this->isVisible()) {
			this->setVisible(false);
		}
		return;
	}
	if (!this->isVisible() && !m_pOnHide) {
		this->setVisible(true);
	}

	if (m_clickPulse > 0.0f) {
		m_clickPulse = std::max(0.0f, m_clickPulse - delta * 4.0f);
	}

	if (m_playLayer->m_player1 && m_pPlayer) {
		m_playerIndex = (m_pPlayer == m_playLayer->m_player1) ? 0 : 1;
	}

	m_hideOnDead = !Mod::get()->getSettingValue<bool>("glowy-on-death");

	// 1. Level end animation: stop producing completely and fade out remaining trail
	if (m_isLevelEnding || m_playLayer->m_hasCompletedLevel) {
		if (!m_parts.empty()) {
			this->onDisappearance(delta);
		}
		return;
	}

	// 2. Dead: stop producing completely and fade out remaining trail
	bool isDead = m_pPlayer->m_isDead || (m_playLayer->m_player1 && m_playLayer->m_player1->m_isDead);
	if (isDead && m_hideOnDead) {
		if (!m_parts.empty()) {
			this->onDisappearance(delta);
		}
		return;
	}

	// 3. Wave state check:
	bool currDartState = m_pPlayer->m_isDart;
	bool isP2Inactive = (m_pPlayer == m_playLayer->m_player2 && !m_playLayer->m_gameState.m_isDualMode);
	bool trailEmpty = (m_pPlayer->m_waveTrail && m_pPlayer->m_waveTrail->m_pointArray) ? (m_pPlayer->m_waveTrail->m_pointArray->count() == 0) : true;

	if (!currDartState || isP2Inactive || m_pPlayer->m_isHidden || trailEmpty) {
		if (!m_parts.empty()) {
			this->onDisappearance(delta);
		}
		prevDartState = currDartState;
		return;
	}

	if (!prevDartState && currDartState) {
		resetTrail();
	}
	prevDartState = currDartState;

	fadeInitialColor.color = Mod::get()->getSettingValue<cocos2d::ccColor3B>(m_playerIndex ? "fade-color-glowy-p2-initial" : "fade-color-glowy-p1-initial");
	fadeFinalColor.color = Mod::get()->getSettingValue<cocos2d::ccColor3B>(m_playerIndex ? "fade-color-glowy-p2-final" : "fade-color-glowy-p1-final");

	Utils::shiftHue(rainbowColor, static_cast<int>(5 * Mod::get()->getSettingValue<double>(m_playerIndex ? "rainbow-glowy-p2-speed" : "rainbow-glowy-p1-speed")), 255, 0);
	Utils::shiftHue(rainbowPastelColor, static_cast<int>(5 * Mod::get()->getSettingValue<double>(m_playerIndex ? "rainbow-pastel-glowy-p2-speed" : "rainbow-pastel-glowy-p1-speed")), 245, 155);
	fadecurrentColor = Utils::interpolateColors(fadeInitialColor, fadeFinalColor, 0.01 * Mod::get()->getSettingValue<double>(m_playerIndex ? "fade-color-glowy-p2-speed" : "fade-color-glowy-p1-speed"));

	cocos2d::CCPoint currentPosition = m_pPlayer->getPosition();
	float distanceMoved = currentPosition.getDistance(m_lastPositionAdded);

	if (distanceMoved > 200.0f) {
		m_lastPositionAdded = currentPosition;
		return;
	}

	const float spacing = 3.5f;
	if (distanceMoved >= spacing) {
		this->onUpdateTrail(currentPosition, distanceMoved);
	}
}

void WaveTrailGlowy::onUpdateTrail(cocos2d::CCPoint currentPosition, float distanceMoved) {
	if (!m_pPlayer) return;

	cocos2d::ccColor3B colorWave = { 255, 255, 255 };
	if (m_pPlayer->m_waveTrail) {
		colorWave = m_pPlayer->m_waveTrail->getColor();
	}

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

	float pulse = 1.0f;
	if (m_pPlayer->m_waveTrail) {
		pulse = m_pPlayer->m_waveTrail->m_pulseSize;
	}

	float smoothed_pulse_size = pulse * (pulse > 1.2f ? 0.6f : (pulse > 0.5f ? 1.0f : 2.0f));

	if (Mod::get()->getSettingValue<bool>("const-size")) {
		smoothed_pulse_size = static_cast<float>(Mod::get()->getSettingValue<double>("size-glowy-wave-trail"));
	}

	bool clickPulseEnabled = Mod::get()->getSettingValue<bool>("click-pulse");
	cocos2d::ccColor3B pulseColor = Mod::get()->getSettingValue<cocos2d::ccColor3B>("click-pulse-color");
	cocos2d::ccColor3B pulsedHeadColor = colorWave;
	if (clickPulseEnabled && m_clickPulse > 0.0f) {
		smoothed_pulse_size *= (1.0f + m_clickPulse * 0.4f);
		float pulseFactor = std::clamp(m_clickPulse, 0.0f, 1.0f);
		float r = static_cast<float>(colorWave.r) + (static_cast<float>(pulseColor.r) - static_cast<float>(colorWave.r)) * pulseFactor;
		float g = static_cast<float>(colorWave.g) + (static_cast<float>(pulseColor.g) - static_cast<float>(colorWave.g)) * pulseFactor;
		float b = static_cast<float>(colorWave.b) + (static_cast<float>(pulseColor.b) - static_cast<float>(colorWave.b)) * pulseFactor;
		pulsedHeadColor.r = static_cast<GLubyte>(std::clamp(r, 0.0f, 255.0f));
		pulsedHeadColor.g = static_cast<GLubyte>(std::clamp(g, 0.0f, 255.0f));
		pulsedHeadColor.b = static_cast<GLubyte>(std::clamp(b, 0.0f, 255.0f));
	}

	bool tailFade = Mod::get()->getSettingValue<bool>("tail-fade-out");
	bool taperTrail = Mod::get()->getSettingValue<bool>("taper-trail");
	float minScale = static_cast<float>(Mod::get()->getSettingValue<double>("taper-min-scale"));
	bool additive = Mod::get()->getSettingValue<bool>("neon-additive-blend");
	cocos2d::ccBlendFunc blendFunc = additive 
		? cocos2d::ccBlendFunc{ GL_SRC_ALPHA, GL_ONE } 
		: cocos2d::ccBlendFunc{ GL_ONE, GL_ONE_MINUS_SRC_ALPHA };
	std::string glowShape = Mod::get()->getSettingValue<std::string>("glow-shape");

	// 1. Remove parts off-screen
	std::vector<cocos2d::CCNode*> partsToRemove = this->getPartsToRemove();
	for (auto& partGlowyToRemove : partsToRemove) {
		if (m_pContainerNode) {
			m_pContainerNode->removeChild(partGlowyToRemove, true);
		}
		m_parts.erase(std::remove(m_parts.begin(), m_parts.end(), partGlowyToRemove), m_parts.end());
		auto it = std::find_if(m_positions.begin(), m_positions.end(), [&](const cocos2d::CCPoint& pos) {
			return pos.x == partGlowyToRemove->getPosition().x && pos.y == partGlowyToRemove->getPosition().y;
		});
		if (it != m_positions.end()) {
			m_positions.erase(it);
		}
	}

	// 2. Spawn equidistant points along the distance moved
	const float spacing = 3.5f;
	int numPoints = static_cast<int>(distanceMoved / spacing);
	numPoints = std::clamp(numPoints, 1, 30);

	cocos2d::CCPoint startPos = m_lastPositionAdded;
	cocos2d::CCPoint dir = (currentPosition - startPos) * (1.0f / distanceMoved);
	for (int p = 1; p <= numPoints; ++p) {
		cocos2d::CCPoint spawnPos = startPos + dir * (spacing * p);
		this->addNewPartGlowy(spawnPos, pulsedHeadColor, smoothed_pulse_size);
	}
	m_lastPositionAdded = startPos + dir * (spacing * numPoints);

	// 3. Update existing parts properties
	size_t count = m_parts.size();
	for (size_t i = 0; i < count; ++i) {
		auto& partGlowy = m_parts[i];
		cocos2d::ccColor3B partColor = colorWave;
		if (clickPulseEnabled && m_clickPulse > 0.0f && (i + 6 >= count)) {
			partColor = pulsedHeadColor;
		}

		if (updateColorAllParts || (clickPulseEnabled && m_clickPulse > 0.0f && i + 6 >= count)) {
			Utils::applySpriteColor(partGlowy, partColor);
		}

		Utils::applySpriteBlend(partGlowy, blendFunc);

		float progress = (count > 1) ? static_cast<float>(i + 1) / static_cast<float>(count) : 1.0f;

		float partScale = smoothed_pulse_size;
		if (taperTrail && count > 1) {
			partScale *= (minScale + (1.0f - minScale) * progress);
		}

		if (glowShape == "Diamond") {
			partGlowy->setRotation(45.0f);
			partGlowy->setScale(partScale);
		}
		else if (glowShape == "Flare") {
			partGlowy->setRotation(30.0f);
			partGlowy->setScaleX(partScale * 1.5f);
			partGlowy->setScaleY(partScale * 0.7f);
		}
		else {
			partGlowy->setRotation(0.0f);
			partGlowy->setScale(partScale);
		}

		float partOpacity = m_pOpacitySpriteCurrent;
		if (clickPulseEnabled && m_clickPulse > 0.0f && (i + 6 >= count)) {
			partOpacity = std::min(255.0f, partOpacity * (1.0f + m_clickPulse * 0.4f));
		}
		if (tailFade && count > 1) {
			partOpacity *= progress;
		}
		Utils::applySpriteOpacity(partGlowy, static_cast<GLubyte>(std::clamp(partOpacity, 0.0f, 255.0f)));
	}
}

std::vector<cocos2d::CCNode*> WaveTrailGlowy::getPartsToRemove() {
	std::vector<cocos2d::CCNode*> ret = {};
	if (!m_playLayer || !m_pPlayer) return ret;

	float camX = m_playLayer->m_gameState.m_cameraPosition.x;
	if (camX == 0.0f && m_playLayer->m_objectLayer) {
		camX = -m_playLayer->m_objectLayer->getPositionX();
	}
	float playerX = m_pPlayer->getPositionX();

	for (auto& partGlowy : m_parts) {
		auto pos = partGlowy->getPosition();
		bool offCameraLeft = ((pos.x - 40.f) - camX) < -80.f;
		bool tooFarBehind = std::abs(playerX - pos.x) > 600.f;
		if (offCameraLeft || tooFarBehind) {
			ret.push_back(partGlowy);
		}
	}

	return ret;
}

void WaveTrailGlowy::onDisappearance(float delta, std::function<void()> callback) {
	m_pOnHide = true;

	auto onComplete = [this, callback]() {
		resetTrail();
		if (callback) {
			callback();
		}
	};

	if (m_pOpacitySpriteCurrent <= 0.0f || m_parts.empty()) {
		onComplete();
	}
	else {
		float fadeDuration = 0.25f;
		float fadeStep = (m_pOpacitySpritesOriginal / fadeDuration) * delta;
		m_pOpacitySpriteCurrent = std::max(0.0f, m_pOpacitySpriteCurrent - fadeStep);

		if (m_pOpacitySpriteCurrent <= 0.0f) {
			onComplete();
			return;
		}

		bool tailFade = Mod::get()->getSettingValue<bool>("tail-fade-out");
		size_t count = m_parts.size();
		for (size_t i = 0; i < count; ++i) {
			float factor = 1.0f;
			if (tailFade && count > 1) {
				factor = static_cast<float>(i + 1) / static_cast<float>(count);
			}
			Utils::applySpriteOpacity(m_parts[i], static_cast<GLubyte>(std::clamp(m_pOpacitySpriteCurrent * factor, 0.0f, 255.0f)));
		}
	}
}

void WaveTrailGlowy::addNewPartGlowy(cocos2d::CCPoint pos, cocos2d::ccColor3B color, float scale) {
	if (!m_pContainerNode) return;

	auto partGlowy = cocos2d::CCSprite::createWithSpriteFrameName("emptyFrame.png");
	if (!partGlowy) {
		partGlowy = cocos2d::CCSprite::create();
	}
	if (!partGlowy) return;

	auto partGlowyTL = cocos2d::CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");
	auto partGlowyTR = cocos2d::CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");
	auto partGlowyBL = cocos2d::CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");
	auto partGlowyBR = cocos2d::CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");

	if (!partGlowyTL || !partGlowyTR || !partGlowyBL || !partGlowyBR) {
		return;
	}

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

	bool additive = Mod::get()->getSettingValue<bool>("neon-additive-blend");
	cocos2d::ccBlendFunc blendFunc = additive 
		? cocos2d::ccBlendFunc{ GL_SRC_ALPHA, GL_ONE } 
		: cocos2d::ccBlendFunc{ GL_ONE, GL_ONE_MINUS_SRC_ALPHA };

	partGlowyTL->setBlendFunc(blendFunc);
	partGlowyTR->setBlendFunc(blendFunc);
	partGlowyBL->setBlendFunc(blendFunc);
	partGlowyBR->setBlendFunc(blendFunc);

	partGlowy->addChild(partGlowyTL);
	partGlowy->addChild(partGlowyTR);
	partGlowy->addChild(partGlowyBL);
	partGlowy->addChild(partGlowyBR);

	partGlowy->setPosition(pos);

	std::string glowShape = Mod::get()->getSettingValue<std::string>("glow-shape");
	if (glowShape == "Diamond") {
		partGlowy->setRotation(45.0f);
		partGlowy->setScale(scale);
	}
	else if (glowShape == "Flare") {
		partGlowy->setRotation(30.0f);
		partGlowy->setScaleX(scale * 1.5f);
		partGlowy->setScaleY(scale * 0.7f);
	}
	else {
		partGlowy->setRotation(0.0f);
		partGlowy->setScale(scale);
	}

	Utils::applySpriteColor(partGlowy, color);
	Utils::applySpriteOpacity(partGlowy, static_cast<GLubyte>(m_pOpacitySpriteCurrent));

	m_pContainerNode->addChild(partGlowy);
	m_parts.push_back(partGlowy);
}

void WaveTrailGlowy::triggerClickPulse() {
	m_clickPulse = 1.0f;
}

void WaveTrailGlowy::startDisappearance() {
	m_pOnHide = true;
}

void WaveTrailGlowy::stopProducingAndDisappear() {
	m_isLevelEnding = true;
	m_pOnHide = true;
}

void WaveTrailGlowy::resetTrail() {
	if (m_pContainerNode) {
		m_pContainerNode->removeAllChildrenWithCleanup(true);
	}
	m_parts.clear();
	m_positions.clear();
	m_pOpacitySpriteCurrent = m_pOpacitySpritesOriginal;
	m_pOnHide = false;
	m_clickPulse = 0.0f;
	if (m_pPlayer) {
		m_lastPositionAdded = m_pPlayer->getPosition();
	}
	else {
		m_lastPositionAdded = { 0, 0 };
	}
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
		if (!this->m_objectLayer) return nullptr;
		return typeinfo_cast<WaveTrailGlowy*>(this->m_objectLayer->getChildByID(player1 ? "WaveTrailGlowy_m_player1" : "WaveTrailGlowy_m_player2"));
	}

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
			return false;
		}

		if (Mod::get()->getSettingValue<bool>("glowy-enabled") && this->m_objectLayer) {
			auto glowWaveTrailP1 = WaveTrailGlowy::create(this, this->m_player1);
			auto glowWaveTrailP2 = WaveTrailGlowy::create(this, this->m_player2);
			if (glowWaveTrailP1 && glowWaveTrailP2) {
				glowWaveTrailP1->setID("WaveTrailGlowy_m_player1");
				glowWaveTrailP2->setID("WaveTrailGlowy_m_player2");
				glowWaveTrailP1->setOpacityGlowy(Utils::convertOpacitySimplf(static_cast<float>(Mod::get()->getSettingValue<double>("opacity-glowy-wave-trail"))));
				glowWaveTrailP2->setOpacityGlowy(Utils::convertOpacitySimplf(static_cast<float>(Mod::get()->getSettingValue<double>("opacity-glowy-wave-trail"))));
				glowWaveTrailP1->setVisible(true);
				glowWaveTrailP2->setVisible(true);
				this->m_objectLayer->addChild(glowWaveTrailP1, -4);
				this->m_objectLayer->addChild(glowWaveTrailP2, -4);
			}
		}

		return true;
	}

	void playEndAnimationToPos(cocos2d::CCPoint position) {
		PlayLayer::playEndAnimationToPos(position);
		if (auto wtp1 = getWaveTrailGlowy(true)) wtp1->stopProducingAndDisappear();
		if (auto wtp2 = getWaveTrailGlowy(false)) wtp2->stopProducingAndDisappear();
	}

	void playPlatformerEndAnimationToPos(cocos2d::CCPoint position, bool instant) {
		PlayLayer::playPlatformerEndAnimationToPos(position, instant);
		if (auto wtp1 = getWaveTrailGlowy(true)) wtp1->stopProducingAndDisappear();
		if (auto wtp2 = getWaveTrailGlowy(false)) wtp2->stopProducingAndDisappear();
	}

	void levelComplete() {
		PlayLayer::levelComplete();
		if (auto wtp1 = getWaveTrailGlowy(true)) wtp1->stopProducingAndDisappear();
		if (auto wtp2 = getWaveTrailGlowy(false)) wtp2->stopProducingAndDisappear();
	}

	void destroyPlayer(PlayerObject* player, GameObject* object) {
		PlayLayer::destroyPlayer(player, object);
		bool glowOnDeath = Mod::get()->getSettingValue<bool>("glowy-on-death");
		if (!glowOnDeath) {
			if (auto wtp1 = getWaveTrailGlowy(true)) wtp1->startDisappearance();
			if (auto wtp2 = getWaveTrailGlowy(false)) wtp2->startDisappearance();
		}
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		auto wtp1 = getWaveTrailGlowy(true);
		auto wtp2 = getWaveTrailGlowy(false);
		if (wtp1) {
			wtp1->m_isLevelEnding = false;
			wtp1->resetTrail();
		}
		if (wtp2) {
			wtp2->m_isLevelEnding = false;
			wtp2->resetTrail();
		}
	}
};

class $modify(PlayerObject) {
	WaveTrailGlowy* getWaveTrailGlowy(bool player1) {
		auto pl = PlayLayer::get();
		if (!pl || !pl->m_objectLayer) {
			return nullptr;
		}
		return typeinfo_cast<WaveTrailGlowy*>(pl->m_objectLayer->getChildByID(player1 ? "WaveTrailGlowy_m_player1" : "WaveTrailGlowy_m_player2"));
	}

	void pushButton(PlayerButton p0) {
		PlayerObject::pushButton(p0);
		if (p0 == PlayerButton::Jump && this->m_isDart) {
			auto pl = PlayLayer::get();
			if (pl) {
				auto glowWave = getWaveTrailGlowy(pl->m_player1 == this);
				if (glowWave) {
					glowWave->triggerClickPulse();
				}
			}
		}
	}

	void playerDestroyed(bool p0) {
		PlayerObject::playerDestroyed(p0);
		auto pl = PlayLayer::get();
		if (pl) {
			bool glowOnDeath = Mod::get()->getSettingValue<bool>("glowy-on-death");
			if (!glowOnDeath) {
				auto glowWave = getWaveTrailGlowy(pl->m_player1 == this);
				if (glowWave) {
					glowWave->startDisappearance();
				}
			}
		}
	}

	void resetStreak() {
		PlayerObject::resetStreak();
		auto pl = PlayLayer::get();
		if (pl) {
			if (pl->m_player1 == this || pl->m_player2 == this) {
				auto glowWave = getWaveTrailGlowy(pl->m_player1 == this);
				if (glowWave) {
					glowWave->resetTrail();
				}
			}
		}
	}
};