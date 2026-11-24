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
	<div class="pd mr border content-box">System Info<br><br>
		<div id="wifiStrength"></div>
		<div id="address"></div>
		<br>
		<div id="firmwareVer"></div>
		<div id="firmwareDt"></div>
		<button onclick="window.location.href='/config'">Configure</button>
		<button onclick="window.location.href='/status'">Status</button>
		<button onclick="window.open('/logs', 'thermostatLogs', 'width=900,height=650')">Live Logs</button>
		<button onclick='window.location.href="/upload"'>Write FW</button>
		<button id="upgradeButton" onclick="upgradeFirmware()">Upgrade FW</button><br>
		<div id="upgradeStatus" aria-live="polite"></div>
		<button onclick=pressButton('terminateTelnet')>Abort Telnet</button>
		<button onclick="if (window.confirm('Clear all saved configuration, including Wi-Fi credentials and Matter/MQTT state, and reset the thermostat to factory defaults?')) pressButton('clearNVS')">Clear Config</button><br>
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
function populateHvacModeButtons(hvacCoolEnable, hvacFanEnable) {
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
		lastHvacControlCapabilities = capabilities;
	}
}

function response() {
	let xmlResponse = xmlHttp.responseXML;
	if (xmlResponse == null) //sometimes the xml response is null?
		return;

	document.getElementById("setTemp").innerHTML = fetchMessage(xmlResponse, "setTemp") + "&deg;" + fetchMessage(xmlResponse, "units");

	document.getElementById("hvacMode").innerHTML = "Set to " + fetchMessage(xmlResponse, "setMode");
	document.getElementById("hvacMode").innerHTML += " [Current action: " + fetchMessage(xmlResponse, "curMode") + "]";

	document.getElementById("curTemp").innerHTML = fetchMessage(xmlResponse, "curTemp") + "&deg;" + fetchMessage(xmlResponse, "units");
	document.getElementById("humidity").innerHTML="Humidity: " + fetchMessage(xmlResponse, "humidity") + "%";
	document.getElementById("light").innerHTML="Light level: " + fetchMessage(xmlResponse, "light");
	document.getElementById("motion").innerHTML="Motion detected: " + fetchMessage(xmlResponse, "motion");

	populateHvacModeButtons(fetchMessage(xmlResponse, 'hvacCoolEnable'), fetchMessage(xmlResponse, 'hvacFanEnable'));

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

const char webStatusUI[] = R"=====(
<!DOCTYPE html>
<html>
<head>
	<meta content="width=device-width,initial-scale=1" name="viewport">
	<title>Thermostat Status</title>
	<style>
		body { background: #202020; color: #ddd; font: 14px monospace; margin: 0; }
		header { background: #303030; padding: 10px; position: sticky; top: 0; }
		.status-layout { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); gap: 16px; padding: 16px; align-items: start; }
		.status-column { display: flex; flex-direction: column; gap: 16px; min-width: 0; }
		.status-box { border: 2px solid #888; border-radius: 5px; background: #303030; min-width: 0; }
		.status-box h2 { margin: 0; padding: 10px 12px; border-bottom: 1px solid #888; font-size: 16px; }
		.status-box pre { margin: 0; padding: 12px; white-space: pre-wrap; overflow-wrap: anywhere; }
		#connectionStatus { margin-left: 12px; }
		@media (max-width: 760px) { .status-layout { grid-template-columns: minmax(0, 1fr); } }
	</style>
</head>
<body>
	<header>
		<strong>Thermostat Status</strong>
		<span id="connectionStatus">Connecting...</span>
		<button onclick="returnToPreviousPage()">Return</button>
	</header>
	<main class="status-layout">
		<section class="status-box">
			<h2>Current Status</h2>
			<pre id="staticStatus">Loading status...</pre>
		</section>
		<div class="status-column">
			<section class="status-box">
				<h2>Runtime Status</h2>
				<pre id="runtimeStatus">Waiting for live status...</pre>
			</section>
			<section class="status-box">
				<h2>Current Error Counts</h2>
				<pre id="errorStatus">Waiting for live status...</pre>
			</section>
		</div>
	</main>
	<script>
		let requestPending = false;
		let staticLoaded = false;
		let statusTempUnits = "F";
		let statusTempCorrection = 0;
		let statusHumidityCorrection = 0;
		let hasMqtt = false;
		let hasMatter = false;
		let hasTelnet = false;
		const staticStatus = document.getElementById("staticStatus");
		const runtimeStatus = document.getElementById("runtimeStatus");
		const errorStatus = document.getElementById("errorStatus");
		const connectionStatus = document.getElementById("connectionStatus");

		function returnToPreviousPage() {
			if (document.referrer && new URL(document.referrer).origin == window.location.origin)
				window.history.back();
			else
				window.location.href = "/";
		}

		function renderStaticStatus(status) {
			statusTempUnits = status.tempUnits;
			statusTempCorrection = status.tempCorrection;
			statusHumidityCorrection = status.humidityCorrection;
			hasMqtt = status.hasMqtt;
			hasMatter = status.hasMatter;
			hasTelnet = status.hasTelnet;
			const lines = [
				"Firmware version: " + status.firmwareVersion,
				"Firmware build date: " + status.firmwareBuildDate,
				"Device name: " + status.deviceName,
				"Friendly name: " + status.friendlyName,
				"MAC Address: " + status.macAddress,
				"Wifi SSID: " + status.wifiSsid,
				"Timezone: " + status.timezone,
				"LD2410 Firmware: " + status.ld2410Firmware,
				"SD Card present: " + (status.sdCardPresent ? "Yes" : "No"),
				"Display sleep time: " + status.displaySleepTime,
				"Touch screen beep: " + (status.touchscreenBeep ? "Enabled" : "Disabled"),
				"Temperature units: " + statusTempUnits,
				"Temperature correction: " + (statusTempCorrection >= 0 ? "+" : "") +
					statusTempCorrection.toFixed(1),
				"Humidity correction: " + (statusHumidityCorrection >= 0 ? "+" : "") +
					statusHumidityCorrection.toFixed(1),
				"Swing temp: " + status.tempSwing.toFixed(1) + " " + statusTempUnits,
				"HVAC modes enabled:",
				"  Heat: True  Cool: " + (status.hvacCoolEnabled ? "True" : "False") +
					"  Fan: " + (status.hvacFanEnabled ? "True" : "False"),
				"  2-stage heating enabled: " + (status.twoStageHeatEnabled ? "True" : "False"),
				"  2-stage cooling enabled: " + (status.twoStageCoolEnabled ? "True" : "False"),
				"  Reversing valve (heat pumps): " + (status.reverseValveEnabled ? "True" : "False")
			];

			if (hasMqtt) {
				lines.push(
					"MQTT Enabled: " + (status.mqttEnabled ? "Yes" : "No"),
					"MQTT Broker: " + status.mqttBroker + "  Port: " + status.mqttPort,
					"MQTT Username: " + status.mqttUsername,
					"MQTT Password: " + (status.mqttPasswordConfigured ? "Configured" : "Not configured")
				);
			}
			if (hasMatter) {
				lines.push(
					"Matter Enabled: " + (status.matterEnabled ? "Yes" : "No")
				);
			}
			staticStatus.textContent = lines.join("\n");
		}

		function renderLiveStatus(status) {
			const runtimeLines = [
				"Uptime: " + status.uptime,
				"Free Heap size: " + status.freeHeapBytes,
				"[Memory] " + status.heapFreePercent.toFixed(1) + "% free - " +
					status.heapFreeBytes + " of " + status.heapTotalBytes + " bytes free",
				"Wifi connected: " + (status.wifiConnected ? "Yes" : "No"),
				"Wifi signal: " + status.wifiSignal + "%",
				"Wifi IP address: " + status.wifiIpAddress,
				"Current time: " + status.currentTime,
				"Light detected: " + status.lightDetected,
				"Motion detected: " + (status.motionDetected ? "Yes" : "No"),
				"Current temp: " + status.currentTemp.toFixed(1) + " " + statusTempUnits +
					" (Correction: " + (statusTempCorrection >= 0 ? "+" : "") +
					statusTempCorrection.toFixed(1) + ")",
				"Current humidity: " + status.currentHumidity.toFixed(1) + "% (Correction: " +
					(statusHumidityCorrection >= 0 ? "+" : "") + statusHumidityCorrection.toFixed(1) + ")",
				"Target temp: " + status.targetTemp.toFixed(1) + " " + statusTempUnits,
				"Current HVAC mode set: " + status.hvacSetMode + " (Currently: " + status.hvacCurrentMode + ")"
			];

			if (hasMqtt)
				runtimeLines.push("MQTT Connected: " + (status.mqttConnected ? "Yes" : "No"));
			if (hasMatter)
				runtimeLines.push("Matter Started: " + (status.matterStarted ? "Yes" : "No"));
			runtimeStatus.textContent = runtimeLines.join("\n");

			const errorLines = [
				"System errors: " + status.systemErrors,
				"Hardware errors: " + status.hardwareErrors,
				"Wifi errors: " + status.wifiErrors
			];
			if (hasMqtt) {
				errorLines.push(
					"MQTT Connect errors: " + status.mqttConnectErrors,
					"MQTT Protocol errors: " + status.mqttProtocolErrors
				);
			}
			if (hasMatter)
				errorLines.push("Matter Connect errors: " + status.matterConnectErrors);
			if (hasTelnet)
				errorLines.push("Telnet network errors: " + status.telnetNetworkErrors);
			errorStatus.textContent = errorLines.join("\n");
		}

		function pollStatus() {
			if (requestPending)
				return;
			requestPending = true;

			const request = new XMLHttpRequest();
			request.onreadystatechange = function() {
				if (request.readyState == XMLHttpRequest.DONE) {
					requestPending = false;
					if (request.status == 200) {
						try {
							const status = JSON.parse(request.responseText);
							if (staticLoaded) {
								renderLiveStatus(status);
							} else {
								renderStaticStatus(status);
								staticLoaded = true;
							}
							connectionStatus.textContent = "Connected";
						} catch (error) {
							console.error(error);
							connectionStatus.textContent = "Invalid status response.";
						}
					} else {
						connectionStatus.textContent = "Connection error; retrying...";
					}
					setTimeout(pollStatus, 1000);
				}
			};
			request.open("GET", "/status?state=" + (staticLoaded ? "live" : "static"), true);
			request.send();
		}

		pollStatus();
	</script>
</body>
</html>
)=====";

const char webConfigUI[] = R"=====(
<!DOCTYPE html>
<html>
<head>
	<meta content="width=device-width,initial-scale=1" name="viewport">
	<title>Thermostat Configuration</title>
	<style>
		body { background: #303030; color: #ddd; font: 16px Arial, sans-serif; margin: 20px; }
		fieldset { border: 1px solid #888; margin: 12px 0; max-width: 700px; }
		label { display: grid; grid-template-columns: minmax(180px, 1fr) minmax(180px, 2fr); gap: 12px; margin: 8px; }
		input, select, button { font: inherit; padding: 5px; }
		input[type=checkbox] { justify-self: start; }
		.actions { margin-top: 16px; }
		#saveStatus { margin: 12px 0; }
	</style>
</head>
<body>
	<h1>Thermostat Configuration</h1>
	<form id="configForm">
		<fieldset>
			<legend>Device and network</legend>
			<label>Device name <input id="deviceName" maxlength="31" required></label>
			<label>Friendly name <input id="friendlyName" maxlength="31" required></label>
			<label>Wi-Fi network name <input id="ssid" maxlength="32"></label>
			<label>Wi-Fi password / PSK <input id="wifiPassword" type="password" maxlength="63" autocomplete="new-password" placeholder="Leave blank to keep current"></label>
			<label>Clear saved Wi-Fi password <input id="clearWifiPassword" type="checkbox"></label>
		</fieldset>
		<fieldset>
			<legend>Temperature and display</legend>
			<label>Temperature units <select id="tempUnits"><option value="F">Fahrenheit</option><option value="C">Celsius</option></select></label>
			<label>Swing temperature <input id="tempSwing" type="number" min="0" max="6" step="0.1" required></label>
			<label>Temperature correction <input id="tempCorrection" type="number" min="-10" max="10" step="0.1" required></label>
			<label>Humidity correction <input id="humidityCorrection" type="number" min="-100" max="100" step="0.1" required></label>
			<label>Display sleep time (seconds) <input id="sleepTime" type="number" min="0" max="65535" step="1" required></label>
			<label>Timezone <select id="timezone"></select></label>
			<label>Touchscreen beep <input id="beep" type="checkbox"></label>
		</fieldset>
		<fieldset>
			<legend>HVAC capabilities</legend>
			<label>Enable cooling <input id="hvacCoolEnable" type="checkbox"></label>
			<label>Enable fan-only mode <input id="hvacFanEnable" type="checkbox"></label>
			<label>Enable two-stage heat / auxiliary heat <input id="twoStageEnable" type="checkbox"></label>
			<label>Enable two-stage cooling <input id="twoStageCoolEnable" type="checkbox"></label>
			<label>Enable reverse valve <input id="reverseEnable" type="checkbox"></label>
		</fieldset>
		<fieldset id="matterSettings" hidden>
			<legend>Matter</legend>
			<label>Enable Matter <input id="matterEnabled" type="checkbox"></label>
		</fieldset>
		<fieldset id="mqttSettings" hidden>
			<legend>MQTT</legend>
			<label>Enable MQTT <input id="mqttEnabled" type="checkbox"></label>
			<label>Broker hostname <input id="mqttHost" maxlength="31"></label>
			<label>Broker port <input id="mqttPort" type="number" min="1" max="65535" step="1"></label>
			<label>Broker username <input id="mqttUsername" maxlength="31"></label>
			<label>Broker password <input id="mqttPassword" type="password" maxlength="71" autocomplete="new-password" placeholder="Leave blank to keep current"></label>
			<label>Clear saved broker password <input id="clearMqttPassword" type="checkbox"></label>
		</fieldset>
		<div id="saveStatus" aria-live="polite"></div>
		<div class="actions">
			<button id="saveButton" type="button" onclick="saveConfig()" disabled>Save</button>
			<button id="rebootButton" type="button" onclick="rebootThermostat()">Reboot</button>
			<button type="button" onclick="window.location.href='/'">Return</button>
		</div>
	</form>
	<script>
		const timezoneNames = [
			"GMT+11", "GMT+10", "GMT+9", "GMT+8", "GMT+7", "GMT+6", "GMT+5", "GMT+4",
			"GMT+3", "GMT+2", "GMT+1", "GMT", "GMT-1", "GMT-2", "GMT-3", "GMT-4",
			"GMT-5", "GMT-6", "GMT-7", "GMT-8", "GMT-9", "GMT-10", "GMT-11", "GMT-12"
		];

		function setField(id, value) {
			document.getElementById(id).value = value;
		}

		function loadConfig() {
			const request = new XMLHttpRequest();
			request.onreadystatechange = function() {
				if (request.readyState == XMLHttpRequest.DONE) {
					if (request.status != 200) {
						document.getElementById("saveStatus").textContent =
							"Could not load configuration (HTTP " + request.status + ").";
						document.getElementById("saveButton").disabled = true;
						return;
					}

					let config;
					try {
						config = JSON.parse(request.responseText);
					} catch (error) {
						console.error(error);
						document.getElementById("saveStatus").textContent = "Could not parse thermostat configuration.";
						document.getElementById("saveButton").disabled = true;
						return;
					}
					setField("deviceName", config.deviceName);
					setField("friendlyName", config.friendlyName);
					setField("ssid", config.ssid);
					setField("tempUnits", config.tempUnits);
					setField("tempSwing", config.tempSwing);
					setField("tempCorrection", config.tempCorrection);
					setField("humidityCorrection", config.humidityCorrection);
					setField("sleepTime", config.sleepTime);
					setField("timezone", config.timezone);
					document.getElementById("beep").checked = config.beep;
					document.getElementById("hvacCoolEnable").checked = config.hvacCoolEnable;
					document.getElementById("hvacFanEnable").checked = config.hvacFanEnable;
					document.getElementById("twoStageEnable").checked = config.twoStageEnable;
					document.getElementById("twoStageCoolEnable").checked = config.twoStageCoolEnable;
					document.getElementById("reverseEnable").checked = config.reverseEnable;

					if (config.hasMatter) {
						document.getElementById("matterSettings").hidden = false;
						document.getElementById("matterEnabled").checked = config.matterEnabled;
					}
					if (config.hasMqtt) {
						document.getElementById("mqttSettings").hidden = false;
						document.getElementById("mqttEnabled").checked = config.mqttEnabled;
						setField("mqttHost", config.mqttHost);
						setField("mqttPort", config.mqttPort);
						setField("mqttUsername", config.mqttUsername);
					}
					document.getElementById("saveButton").disabled = false;
				}
			};
			request.open("GET", "/config?state=1", true);
			request.send();
		}

		function saveConfig() {
			if (!document.getElementById("configForm").reportValidity())
				return;

			const config = {
				deviceName: document.getElementById("deviceName").value,
				friendlyName: document.getElementById("friendlyName").value,
				ssid: document.getElementById("ssid").value,
				wifiPassword: document.getElementById("wifiPassword").value,
				clearWifiPassword: document.getElementById("clearWifiPassword").checked,
				tempUnits: document.getElementById("tempUnits").value,
				tempSwing: Number(document.getElementById("tempSwing").value),
				tempCorrection: Number(document.getElementById("tempCorrection").value),
				humidityCorrection: Number(document.getElementById("humidityCorrection").value),
				sleepTime: Number(document.getElementById("sleepTime").value),
				timezone: Number(document.getElementById("timezone").value),
				beep: document.getElementById("beep").checked,
				hvacCoolEnable: document.getElementById("hvacCoolEnable").checked,
				hvacFanEnable: document.getElementById("hvacFanEnable").checked,
				twoStageEnable: document.getElementById("twoStageEnable").checked,
				twoStageCoolEnable: document.getElementById("twoStageCoolEnable").checked,
				reverseEnable: document.getElementById("reverseEnable").checked
			};
			if (document.getElementById("matterSettings").hidden == false)
				config.matterEnabled = document.getElementById("matterEnabled").checked;
			if (document.getElementById("mqttSettings").hidden == false) {
				config.mqttEnabled = document.getElementById("mqttEnabled").checked;
				config.mqttHost = document.getElementById("mqttHost").value;
				config.mqttPort = Number(document.getElementById("mqttPort").value);
				config.mqttUsername = document.getElementById("mqttUsername").value;
				config.mqttPassword = document.getElementById("mqttPassword").value;
				config.clearMqttPassword = document.getElementById("clearMqttPassword").checked;
			}

			document.getElementById("saveButton").disabled = true;
			document.getElementById("saveStatus").textContent = "Saving configuration...";
			const request = new XMLHttpRequest();
			request.onreadystatechange = function() {
				if (request.readyState == XMLHttpRequest.DONE) {
					document.getElementById("saveButton").disabled = false;
					document.getElementById("saveStatus").textContent =
						request.status == 200 ? request.responseText :
						"Save failed (HTTP " + request.status + "): " + request.responseText;
				}
			};
			request.open("POST", "/config", true);
			request.setRequestHeader("Content-Type", "application/json");
			request.send(JSON.stringify(config));
		}

		function rebootThermostat() {
			if (!window.confirm("Reboot the thermostat now? Unsaved changes will be lost."))
				return;

			const button = document.getElementById("rebootButton");
			button.disabled = true;
			document.getElementById("saveStatus").textContent = "Rebooting thermostat...";
			const request = new XMLHttpRequest();
			request.onreadystatechange = function() {
				if (request.readyState == XMLHttpRequest.DONE && request.status != 200) {
					button.disabled = false;
					document.getElementById("saveStatus").textContent =
						"Reboot request failed (HTTP " + request.status + ").";
				}
			};
			request.open("POST", "/reboot", true);
			request.send();
		}

		for (let index = 0; index < timezoneNames.length; ++index) {
			const option = document.createElement("option");
			option.value = index;
			option.textContent = timezoneNames[index];
			document.getElementById("timezone").appendChild(option);
		}
		loadConfig();
	</script>
</body>
</html>
)=====";

const char webLogUI[] = R"=====(
<!DOCTYPE html>
<html>
<head>
	<meta content="width=device-width,initial-scale=1" name="viewport">
	<title>Thermostat Live Logs</title>
	<style>
		body { background: #202020; color: #ddd; font: 14px monospace; margin: 0; }
		header { background: #303030; padding: 10px; position: sticky; top: 0; }
		#logOutput { height: calc(100vh - 60px); white-space: pre-wrap; overflow-wrap: anywhere; overflow: auto; padding: 10px; margin: 0; color: #ddd; }
		#connectionStatus { margin-left: 12px; }
	</style>
</head>
<body>
	<header>
		<strong>Live firmware logs</strong>
		<span id="connectionStatus">Connecting...</span>
		<button onclick="clearLogs()">Clear view</button>
		<span>Log level follows the Web Log (weblog) setting.</span>
	</header>
	<pre id="logOutput"></pre>
	<script>
		let cursor = "0";
		let requestPending = false;
		let pendingAnsiSequence = "";
		let ansiColor = null;
		let ansiBold = false;
		const ansiColors = {
			30: "#777", 31: "#f55", 32: "#5f5", 33: "#ff5",
			34: "#69f", 35: "#e6e", 36: "#5df", 37: "#eee",
			90: "#aaa", 91: "#f88", 92: "#8f8", 93: "#ff8",
			94: "#9af", 95: "#f8f", 96: "#8ff", 97: "#fff"
		};
		const logOutput = document.getElementById("logOutput");
		const connectionStatus = document.getElementById("connectionStatus");

		function appendLogSegment(text) {
			if (text.length == 0)
				return;

			if (ansiColor == null && !ansiBold) {
				logOutput.appendChild(document.createTextNode(text));
				return;
			}

			const span = document.createElement("span");
			if (ansiColor != null)
				span.style.color = ansiColor;
			if (ansiBold)
				span.style.fontWeight = "bold";
			span.textContent = text;
			logOutput.appendChild(span);
		}

		function applyAnsiCodes(codes) {
			const values = codes == "" ? [0] : codes.split(";").map(Number);
			for (const value of values) {
				if (value == 0) {
					ansiColor = null;
					ansiBold = false;
				} else if (value == 1) {
					ansiBold = true;
				} else if (value == 22) {
					ansiBold = false;
				} else if (value == 39) {
					ansiColor = null;
				} else if (ansiColors[value] != null) {
					ansiColor = ansiColors[value];
				}
			}
		}

		function appendAnsiLog(text) {
			text = pendingAnsiSequence + text;
			pendingAnsiSequence = "";
			let segmentStart = 0;
			let outputEnd = text.length;
			let index = 0;
			while (index < text.length) {
				if (text.charCodeAt(index) == 27 && index + 1 == text.length) {
					pendingAnsiSequence = text.slice(index);
					outputEnd = index;
					break;
				}
				if (text.charCodeAt(index) != 27 || text[index + 1] != "[") {
					index++;
					continue;
				}

				let end = index + 2;
				while (end < text.length && /[0-9;]/.test(text[end]))
					end++;
				if (end == text.length) {
					if (end - index <= 32) {
						pendingAnsiSequence = text.slice(index);
						outputEnd = index;
						break;
					}
					index++;
					continue;
				}
				if (text[end] != "m") {
					index++;
					continue;
				}

				appendLogSegment(text.slice(segmentStart, index));
				applyAnsiCodes(text.slice(index + 2, end));
				index = end + 1;
				segmentStart = index;
			}
			appendLogSegment(text.slice(segmentStart, outputEnd));
		}

		function trimLogOutput() {
			let excess = logOutput.textContent.length - 49152;
			while (excess > 0 && logOutput.firstChild != null) {
				const first = logOutput.firstChild;
				const length = first.textContent.length;
				if (length <= excess) {
					logOutput.removeChild(first);
					excess -= length;
				} else {
					first.textContent = first.textContent.slice(excess);
					excess = 0;
				}
			}
		}

		function clearLogs() {
			logOutput.textContent = "";
			pendingAnsiSequence = "";
			ansiColor = null;
			ansiBold = false;
		}

		function pollLogs() {
			if (requestPending)
				return;
			requestPending = true;

			const request = new XMLHttpRequest();
			request.onreadystatechange = function() {
				if (request.readyState == XMLHttpRequest.DONE) {
					requestPending = false;
					if (request.status == 200) {
						const nextCursor = request.getResponseHeader("X-Log-Next");
						if (nextCursor !== null)
							cursor = nextCursor;
						if (request.responseText.length > 0) {
							appendAnsiLog(request.responseText);
							if (logOutput.textContent.length > 65536)
								trimLogOutput();
							logOutput.scrollTop = logOutput.scrollHeight;
						}
						connectionStatus.textContent = "Connected";
					} else {
						connectionStatus.textContent = "Connection error; retrying...";
					}
					setTimeout(pollLogs, 500);
				}
			};
			request.open("GET", "/logdata?since=" + encodeURIComponent(cursor), true);
			request.send();
		}

		pollLogs();
	</script>
</body>
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