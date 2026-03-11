import math
import os
from datetime import datetime
from flask import Flask, request, jsonify, render_template

app = Flask(__name__)

FILE="motor_state.txt"

def load_motor():
    if not os.path.exists(FILE):
        with open(FILE,"w") as f:
            f.write("0,0")
        return {"sn":0,"ew":0}

    with open(FILE) as f:
        sn,ew=f.read().split(",")
        return {"sn":int(sn),"ew":int(ew)}

def save_motor(sn,ew):
    with open(FILE,"w") as f:
        f.write(f"{sn},{ew}")

motor_state=load_motor()

sensor={"voltage":0,"temp":0,"gas":0}

hour_data=[]
avg_voltage=0
cleaning=False
pump=False


@app.route("/")
def home():
    return render_template("index.html")


@app.route("/esp",methods=["POST"])
def esp():

    global avg_voltage,cleaning,pump

    data=request.get_json()

    sensor["voltage"]=data["voltage"]
    sensor["temp"]=data["temp"]
    sensor["gas"]=data["gas"]

    hour_data.append(sensor["voltage"])

    if len(hour_data)>=60:
        avg_voltage=sum(hour_data)/len(hour_data)
        hour_data.clear()

    if avg_voltage>0:
        cleaning=sensor["voltage"]<(avg_voltage*0.3)

    hour=datetime.now().hour

    pump = hour>=18

    return jsonify(ok=True)


@app.route("/status")
def status():

    return jsonify(
        voltage=sensor["voltage"],
        avg_voltage=avg_voltage,
        sn=motor_state["sn"],
        ew=motor_state["ew"],
        cleaning=cleaning,
        pump=pump
    )


@app.route("/set_target",methods=["POST"])
def set_target():

    data=request.get_json()

    sn=int(data["sn"])
    ew=int(data["ew"])

    motor_state["sn"]=sn
    motor_state["ew"]=ew

    save_motor(sn,ew)

    return jsonify(ok=True)


@app.route("/get_target")
def get_target():

    return jsonify(
        sn=motor_state["sn"],
        ew=motor_state["ew"],
        cleaning=cleaning,
        pump=pump
    )


@app.route("/motor_done",methods=["POST"])
def motor_done():

    data=request.get_json()

    sn=data["sn"]
    ew=data["ew"]

    motor_state["sn"]=sn
    motor_state["ew"]=ew

    save_motor(sn,ew)

    return jsonify(ok=True)


@app.route("/solar",methods=["POST"])
def solar_api():

    data=request.get_json()

    lat=float(data["lat"])
    lon=float(data["lon"])

    date=data["date"]
    time=data["time"]

    dt=datetime.strptime(date+" "+time,"%Y-%m-%d %H:%M")

    az,ze=solar_azimuth_zenith(lat,lon,dt)

    sn = sn_angle_from_zenith(ze)
    ew = ew_angle_from_azimuth(az)

    # stop tracker after sunset
    if ze > 90:
        sn = 0
        ew = 0

    motor_state["sn"]=sn
    motor_state["ew"]=ew

    save_motor(sn,ew)

    print("Solar target:",sn,ew)

    return jsonify(
        azimuth=round(az,2),
        zenith=round(ze,2),
        sn=sn,
        ew=ew
    )


def ew_angle_from_azimuth(az):
    # convert azimuth to -45 to +45
    ew = (az - 180) / 4

    ew = max(-45, min(45, ew))

    return round(ew)


def sn_angle_from_zenith(ze):
    altitude = 90 - ze

    # map altitude to -45 → +45
    sn = (altitude - 45)

    sn = max(-45, min(45, sn))

    return round(sn)


def solar_azimuth_zenith(lat,lon,date_time):

    lat_rad=math.radians(lat)

    n=date_time.timetuple().tm_yday
    hour=date_time.hour+date_time.minute/60

    decl=math.radians(23.45)*math.sin(
        math.radians(360/365*(284+n))
    )

    B=math.radians(360/365*(n-81))

    EoT=9.87*math.sin(2*B)-7.53*math.cos(B)-1.5*math.sin(B)

    LSTM=15*round(lon/15)

    TC=4*(lon-LSTM)+EoT

    LST=hour+TC/60

    H=math.radians(15*(LST-12))

    cos_z=(math.sin(lat_rad)*math.sin(decl)
    +math.cos(lat_rad)*math.cos(decl)*math.cos(H))

    zenith=math.degrees(math.acos(cos_z))

    sin_az=-(math.sin(H)*math.cos(decl))/math.sin(math.radians(zenith))

    cos_az=(math.sin(decl)-math.sin(lat_rad)*math.cos(math.radians(zenith)))/(math.cos(lat_rad)*math.sin(math.radians(zenith)))

    azimuth=math.degrees(math.atan2(sin_az,cos_az))
    azimuth=(azimuth+360)%360

    return azimuth,zenith


if __name__=="__main__":
    app.run(host="0.0.0.0",port=5000,debug=True)