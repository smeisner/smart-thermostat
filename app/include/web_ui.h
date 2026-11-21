const char webUI[] = R"=====(
<html>
<meta content="width=device-width,initial-scale=1"name=viewport>
<link rel="icon" href="data:,">
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
		<div class=center id="thermoControlBox">Thermostat Controls</div>
			<div class=flx>
				<div class="temp ctrl" id="setTemp"></div>
				<div class="pd mr">
					<button onclick=pressButton('tempUp')>&uarr;</button><br>
					<button onclick=pressButton('tempDown')>&darr;</button>
				</div>
			</div>
		<br>
		<p id="hvacMode"></p>
		<span id="hvacButtons"></span>
		<div id="buttonStatus" aria-live="polite"></div>
	</div>
	<div class="pd mr border content-box">
		Current Readings
		<div class="pd mr temp" id="curTemp"></div>
		<div id="humidity"></div>
		<div id="light"></div>
		<div id="motion"></div>
	</div>
	<div class="pd mr border content-box">Thermostat Settings
	<br><br><br>
		<span id="units"></span><button onclick=pressButton('unitToggle')>toggle</button><br>
		<span id="swing"></span>
		<button onclick=pressButton('swingUp')>&uarr;</button>
		<button onclick=pressButton('swingDown')>&darr;</button><br>
		<span id="correction"></span>
		<button onclick=pressButton('correctionUp')>&uarr;</button>
		<button onclick=pressButton('correctionDown')>&darr;</button>
	</div>
	<div class="pd mr border content-box">HVAC system settings
	<br><br><br>
		<label for="coolCheckBox">Enable HVAC Cooling</label>
		<span id="coolCheckBox"></span>
		<br>
		<label for="fanCheckBox">Enable HVAC Fan</label>
		<span id="fanCheckBox"></span>
		<br>
		<label for="auxHeatCheckBox">Enable Auxillary Heat</label>
		<span id="auxHeatCheckBox"></span>
		<br>
		<label for="reverseCheckBox">Enable Reverse Valve</label>
		<span id="reverseCheckBox"></span>
	</div>
	<div class="pd mr border content-box">System Info<br><br>
		<div id="wifiStrength"></div>
		<div id="address"></div>
		<br>
		<div id="firmwareVer"></div>
		<div id="firmwareDt"></div>
		<button onclick='window.location.href="/upload"'>Write FW</button>
		<button id="upgradeButton" onclick="upgradeFirmware()">Upgrade FW</button><br>
		<div id="upgradeStatus" aria-live="polite"></div>
		<button onclick=pressButton('terminateTelnet')>Abort Telnet</button>
		<button onclick=pressButton('clearNVS')>Clear Config</button><br>
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
let queuedButtonCommands = [];
let buttonCommandActive = false;
function pressButton(buttonID) {
	queuedButtonCommands.push(buttonID);
	sendNextButtonCommand();
}

function sendNextButtonCommand() {
	if (buttonCommandActive || queuedButtonCommands.length == 0)
		return;

	buttonCommandActive = true;
	let buttonID = queuedButtonCommands.shift();
	let xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function() {
		if (xhttp.readyState == XMLHttpRequest.DONE) {
			buttonCommandActive = false;
			if (xhttp.status != 200) {
				document.getElementById("buttonStatus").textContent =
					"Button request failed (HTTP " + xhttp.status + ").";
			} else if (queuedButtonCommands.length == 0) {
				document.getElementById("buttonStatus").textContent = "";
			}
			sendNextButtonCommand();
		}
	};
	xhttp.open("PUT", "/button", true);
	xhttp.send(buttonID);
}

function upgradeFirmware() {
	let button = document.getElementById("upgradeButton");
	let status = document.getElementById("upgradeStatus");
	button.disabled = true;
	status.textContent = "Checking for a firmware upgrade...";

	let xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function() {
		if (xhttp.readyState == XMLHttpRequest.DONE) {
			button.disabled = false;
			status.textContent = xhttp.status == 200
				? xhttp.responseText
				: "Firmware upgrade request failed (HTTP " + xhttp.status + ").";
		}
	};
	xhttp.open("POST", "/gh_upgrade", true);
	xhttp.send();
}

function fetchMessage(xmlResponse, tag) {
	let xmlDoc = xmlResponse.getElementsByTagName(tag);
	return xmlDoc[0].firstChild.nodeValue;
}

let lastHvacControlCapabilities = null;
function populateOptionalHvacSettings(hvacCoolEnable, hvacFanEnable, twoStageEnable, reverseEnable) {
	let capabilities = hvacCoolEnable + ":" + hvacFanEnable;
	if (capabilities != lastHvacControlCapabilities) {
		let hvacButtons = "<button onclick=pressButton('hvacModeOff')>OFF</button>";
		if (hvacCoolEnable == "1") {
			hvacButtons += "<button onclick=pressButton('hvacModeAuto')>AUTO</button>";
			hvacButtons += "<button onclick=pressButton('hvacModeHeat')>HEAT</button>";
			hvacButtons += "<button onclick=pressButton('hvacModeCool')>COOL</button>";
		} else {
			hvacButtons += "<button onclick=pressButton('hvacModeHeat')>HEAT</button>";
		}
		if (hvacFanEnable == "1") {
			hvacButtons += "<button onclick=pressButton('hvacModeFan')>FAN</button>";
		}
		document.getElementById("hvacButtons").innerHTML = hvacButtons;
		document.getElementById("coolCheckBox").innerHTML =
			"<input type='checkbox' onclick=pressButton('hvacCoolEnable')>";
		document.getElementById("fanCheckBox").innerHTML =
			"<input type='checkbox' onclick=pressButton('hvacFanEnable')>";
		lastHvacControlCapabilities = capabilities;
	}

	document.querySelector("#coolCheckBox input").checked = hvacCoolEnable == "1";
	document.querySelector("#fanCheckBox input").checked = hvacFanEnable == "1";
	let twoStageHTML = "<input type='checkbox' onclick=pressButton('twoStageEnable')";
	twoStageHTML += twoStageEnable == "1" ? " checked>" : ">";
	let reverseHTML = "<input type='checkbox' onclick=pressButton('reverseEnable')";
	reverseHTML += reverseEnable == "1" ? " checked>" : ">";
	if (document.querySelector("#auxHeatCheckBox input") == null)
		document.getElementById("auxHeatCheckBox").innerHTML = twoStageHTML;
	else
		document.querySelector("#auxHeatCheckBox input").checked = twoStageEnable == "1";
	if (document.querySelector("#reverseCheckBox input") == null)
		document.getElementById("reverseCheckBox").innerHTML = reverseHTML;
	else
		document.querySelector("#reverseCheckBox input").checked = reverseEnable == "1";
}

function populateHvacSliders(correction, swing) {
	document.getElementById("correction").innerHTML="Temp Correction: " + correction;
	document.getElementById("swing").innerHTML="Temp Swing: " + swing;
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
	document.getElementById("motion").innerHTML="Motion detected: " + fetchMessage(xmlResponse, "motion");

	document.getElementById("units").innerHTML="Temp Units: " + fetchMessage(xmlResponse, "units");
	populateHvacSliders(fetchMessage(xmlResponse, "correction"), fetchMessage(xmlResponse, "swing"));

	populateOptionalHvacSettings(fetchMessage(xmlResponse, 'hvacCoolEnable'), fetchMessage(xmlResponse, 'hvacFanEnable'),
			fetchMessage(xmlResponse, 'twoStageEnable'), fetchMessage(xmlResponse, 'reverseEnable'));

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
	setTimeout(process, 1000);
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