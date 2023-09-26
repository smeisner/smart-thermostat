const char webUI[] = R"=====(
<html>
<meta content="width=device-width,initial-scale=1"name=viewport>
<title>Truly Smart Thermostat</title>
<style>body{background-color:#303030;font-family:Arial,Helvetica,Sans-Serif;Color:#cacaca}
a{color:#006eff}h2{padding:0;margin:0}.mr{margin:10px}.pd{padding:10px}
p{margin:0px;padding:0px}
.border{border:2px solid;border-radius:5px}.center{text-align:center}
.flx{display:flex;flex-wrap:wrap;justify-content:center}.content-box{width:fit-content;float:left}
.ctrl{margin-left:15px;margin-top:15px;margin-right:50px}.temp{font-size:72px}
.arrow{font-size:25px}button{margin:5px}</style>
<body onload="process()">
<h1 class="pd border center">Smart Thermostat Control Panel</h1>
<div class="center flx">
    <div class="pd mr border content-box">
        <div class=center>Thermostat Controls</div>
        <div class=flx>
            <div class="temp ctrl" id="setTemp"></div>
            <div class="pd mr">
                <button onclick=pressButton('tempUp')>&uarr;</button><br>
                <button onclick=pressButton('tempDown')>&darr;</button>
            </div>
        </div>
    <p id="hvacMode"></p>
    <button onclick=pressButton('hvacModeOff')>OFF</button>
    <button onclick=pressButton('hvacModeAuto')>AUTO</button>
    <button onclick=pressButton('hvacModeHeat')>HEAT</button>
    <button onclick=pressButton('hvacModeCool')>COOL</button>
    <button onclick=pressButton('hvacModeFan')>FAN</button>
</div>
<div class="pd mr border content-box">
    Current Readings
    <div class="pd mr temp" id="curTemp"></div>
    <div id="humidity"></div>
    <div id="light"></div>
    <div id="motion"></div>
</div>
<div class="pd mr border content-box">Thermostat Settings
<br><br><br><br>
<div id="units"></div>
<div id="swing"></div>
<div id="correction"></div>
</div>
<div class="pd mr border content-box">System Settings<br><br>
<div id="wifiStrength"></div>
<div id="address"></div>
<br>
<div id="firmwareVer"></div>
<div id="firmwareDt"></div>
<button onclick='window.location.href="/upload"'>Update Firmware</button>
<br><button onclick=pressButton('clearFirmware')>Clear Config</button><br>
</div>
</div><br><br><br>
<footer>
<div id="copyright"></div>
<a href=https://github.com/smeisner/smart-thermostat target=_blank>Smart Thermostat Project on GitHub</a>
</footer>
</body>

<script>
function createXmlHttpObject() {
    if (window.XMLHttpRequest) {
        return new XMLHttpRequest();
    }
    else {
        return new ActiveXObject("Microsoft.XMLHTTP");
    }
}
let xmlHttp = createXmlHttpObject();
function pressButton(buttonID) {
    let xhttp = new XMLHttpRequest();
    xhttp.open('PUT', "/button", false);
    xhttp.send(buttonID);
}

function fetchMessage(xmlResponse, tag) {
    let xmlDoc = xmlResponse.getElementsByTagName(tag);
    return xmlDoc[0].firstChild.nodeValue;
}

function response() {
    let xmlResponse = xmlHttp.responseXML;
    if (xmlResponse == null) //sometimes the xml response is null?
        return;

    let xmlDoc;
    let message;

    document.getElementById("setTemp").innerHTML = fetchMessage(xmlResponse, "setTemp") + "&deg;" + fetchMessage(xmlResponse, "units");

    document.getElementById("hvacMode").innerHTML = "Set to " + fetchMessage(xmlResponse, "setMode");
    document.getElementById("hvacMode").innerHTML += " [Current action: " + fetchMessage(xmlResponse, "curMode") + "]";

    document.getElementById("curTemp").innerHTML = fetchMessage(xmlResponse, "curTemp") + "&deg;" + fetchMessage(xmlResponse, "units");
    document.getElementById("humidity").innerHTML="Humidity: " + fetchMessage(xmlResponse, "humidity") + "%";
    document.getElementById("light").innerHTML="Light level: " + fetchMessage(xmlResponse, "light");
    document.getElementById("motion").innerHTML="Motion detected?: " + fetchMessage(xmlResponse, "motion");

    document.getElementById("units").innerHTML="Temp Units: " + fetchMessage(xmlResponse, "units");
    document.getElementById("swing").innerHTML="Temp Swing: " + fetchMessage(xmlResponse, "swing");
    document.getElementById("correction").innerHTML="Temp Correction: " + fetchMessage(xmlResponse, "correction");

    document.getElementById("wifiStrength").innerHTML="Wifi Signal Strength: " + fetchMessage(xmlResponse, "wifiStrength");
    document.getElementById("address").innerHTML="IP: " + fetchMessage(xmlResponse, "address");

    document.getElementById("firmwareVer").innerHTML= "Firmware: " + fetchMessage(xmlResponse, "firmwareVer");
    document.getElementById("firmwareDt").innerHTML= "Built: " + fetchMessage(xmlResponse, "firmwareDt");

    document.getElementById("copyright").innerHTML=fetchMessage(xmlResponse, "copyright");
}
function process() {
    if (xmlHttp.readyState==0 || xmlHttp.readyState==4) {
        xmlHttp.open("PUT", "xml", true);
        xmlHttp.onreadystatechange=response;
        xmlHttp.send(null);
    }
    setTimeout("process()",200);
}
</script>
</html>
)=====";

const char *web_fw_upload = R"=====(
<!DOCTYPE html>
<html>
	<head>
		<meta http-equiv="content-type" content="text/html; charset=utf-8" />
		<title>ESP32 OTA Update</title>
		<script>
			function startUpload() {
				var otafile = document.getElementById("otafile").files;
				if (otafile.length == 0) {
					alert("No file selected!");
				} else {
					document.getElementById("otafile").disabled = true;
					document.getElementById("upload").disabled = true;
					var file = otafile[0];
					var xhr = new XMLHttpRequest();
					xhr.onreadystatechange = function() {
						if (xhr.readyState == 4) {
							if (xhr.status == 200) {
								document.open();
								document.write(xhr.responseText);
								document.close();
							} else if (xhr.status == 0) {
								alert("Server closed the connection abruptly!");
								location.reload()
							} else {
								alert(xhr.status + " Error!\n" + xhr.responseText);
								location.reload()
							}
						}
					};
					xhr.upload.onprogress = function (e) {
						var progress = document.getElementById("progress");
						progress.textContent = "Progress: " + (e.loaded / e.total * 100).toFixed(0) + "%";
					};
					xhr.open("POST", "/update", true);
					xhr.send(file);
				}
			}
		</script>
	</head>
	<body>
		<h1>ESP32 OTA Firmware Update</h1>
		<div>
			<label for="otafile">Firmware file:</label>
			<input type="file" id="otafile" name="otafile" />
		</div>
		<div>
			<button id="upload" type="button" onclick="startUpload()">Upload</button>
		</div>
		<div id="progress"></div>
	</body>
</html>)=====";