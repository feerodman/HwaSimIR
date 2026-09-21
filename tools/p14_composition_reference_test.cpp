#include <cmath>
#include <iostream>

namespace {

double SensorPlane(double tau, double source, double path)
{
    return tau * source + path;
}

double StraightAlpha(double destination, double source, double alpha)
{
    return source * alpha + destination * (1.0 - alpha);
}

double WhiteHot(double radiance, double low, double high)
{
    const double normalized = (radiance - low) / (high - low);
    return normalized < 0.0 ? 0.0 : normalized > 1.0 ? 1.0 : normalized;
}

bool Near(double left, double right, double tolerance = 1.0e-12)
{
    return std::fabs(left - right) <= tolerance;
}

} // namespace

int main()
{
    const double tau = 0.70;
    const double path = 0.20;
    const double backgroundSource = 3.0;
    const double hotSource = 10.0;
    const double coldSmokeSource = 0.5;
    const double alpha = 0.40;
    const double backgroundSensor = SensorPlane(tau, backgroundSource, path);
    const double hotSensor = SensorPlane(tau, hotSource, path);
    const double coldSensor = SensorPlane(tau, coldSmokeSource, path);
    const double hotComposite = StraightAlpha(backgroundSensor, hotSensor, alpha);
    const double coldComposite = StraightAlpha(backgroundSensor, coldSensor, alpha);
    const double expectedHot = tau * ((1.0 - alpha) * backgroundSource + alpha * hotSource) + path;
    const double expectedCold = tau * ((1.0 - alpha) * backgroundSource + alpha * coldSmokeSource) + path;
    const double hotSigned = hotComposite - backgroundSensor;
    const double coldSigned = coldComposite - backgroundSensor;
    const double whiteHotSigned = WhiteHot(hotComposite, 0.0, 8.0) - WhiteHot(backgroundSensor, 0.0, 8.0);
    const double blackHotSigned = (1.0 - WhiteHot(hotComposite, 0.0, 8.0)) -
        (1.0 - WhiteHot(backgroundSensor, 0.0, 8.0));
    const bool pass = Near(hotComposite, expectedHot) && Near(coldComposite, expectedCold) &&
        hotSigned > 0.0 && coldSigned < 0.0 && whiteHotSigned > 0.0 && blackHotSigned < 0.0;
    std::cout << "[P14CompositionReference] unit=W/(m^2_sr_um) tau=" << tau
              << " path=" << path << " alpha=" << alpha
              << " backgroundSensor=" << backgroundSensor
              << " hotSourceSensor=" << hotSensor
              << " coldSmokeSensor=" << coldSensor
              << " hotSignedRawDelta=" << hotSigned
              << " coldSmokeSignedRawDelta=" << coldSigned
              << " whiteHotDisplaySignedDelta=" << whiteHotSigned
              << " blackHotDisplaySignedDelta=" << blackHotSigned
              << " pathPreservedExactly=" << (Near(hotComposite, expectedHot) && Near(coldComposite, expectedCold) ? 1 : 0)
              << " absoluteDifferenceIsNotSignEvidence=1"
              << " result=" << (pass ? "PASS" : "FAIL") << std::endl;
    return pass ? 0 : 1;
}
