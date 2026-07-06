"""Water-stress decision — the AI 'brain' for feature 1 (rule-based, no model).

Stateless pure function: given already-averaged sensor inputs and the thresholds
(both supplied by the web-server, which owns aggregation + settings), return the
risk band + label + human-readable factors. Ported from the web-server's original
Node engine; this is now the ONE place the decision lives.

Soil moisture is required; temperature + humidity are optional (missing => soil
alone). Runs on Python 3.6 (stdlib only).
"""

RISK_BY_BAND = {1: "low", 2: "medium", 3: "high"}

# Fallback thresholds if the caller omits any (web-server normally sends all).
DEFAULT_THRESHOLDS = {
    "soilMediumBelow": 60,
    "soilHighBelow": 30,
    "hotAtOrAbove": 33,
    "dryAtOrBelow": 45,
    "coolAtOrBelow": 22,
    "humidAtOrAbove": 75,
}


def _num(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def decide(inputs, thresholds):
    t = dict(DEFAULT_THRESHOLDS)
    t.update(thresholds or {})
    inputs = inputs or {}
    soil = _num(inputs.get("soilMoisture"))
    temp = _num(inputs.get("temperature"))
    humidity = _num(inputs.get("humidity"))

    if soil is None:
        return {
            "band": None,
            "risk": "unknown",
            "factors": ["No fresh soil-moisture reading — cannot estimate water stress."],
        }

    band = 3 if soil < t["soilHighBelow"] else 2 if soil < t["soilMediumBelow"] else 1
    factors = ["Soil moisture %d%% -> base %s." % (round(soil), RISK_BY_BAND[band])]

    if temp is not None and humidity is not None:
        if temp >= t["hotAtOrAbove"] and humidity <= t["dryAtOrBelow"] and band < 3:
            band += 1
            factors.append(
                "Hot & dry (%d°C / %d%%RH) raised it to %s." % (round(temp), round(humidity), RISK_BY_BAND[band])
            )
        elif temp <= t["coolAtOrBelow"] and humidity >= t["humidAtOrAbove"] and band > 1:
            band -= 1
            factors.append(
                "Cool & humid (%d°C / %d%%RH) lowered it to %s." % (round(temp), round(humidity), RISK_BY_BAND[band])
            )
        else:
            factors.append(
                "Air %d°C / %d%%RH — no evaporative-demand adjustment." % (round(temp), round(humidity))
            )
    else:
        factors.append("Temperature/humidity unavailable — using soil moisture alone.")

    return {"band": band, "risk": RISK_BY_BAND[band], "factors": factors}
