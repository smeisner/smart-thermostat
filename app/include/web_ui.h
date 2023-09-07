const char *webUI = "\
<meta content=\"width=device-width,initial-scale=1\"name=viewport>\
<meta content=5 http-equiv=refresh>\
<title>Truly Smart Thermostat</title>\
<style>body{background-color:#303030;font-family:Arial,Helvetica,Sans-Serif;Color:#cacaca}\
a{color:#006eff}h2{padding:0;margin:0}.mr{margin:10px}.pd{padding:10px}\
.border{border:2px solid;border-radius:5px}.center{text-align:center}\
.flx{display:flex;flex-wrap:wrap;justify-content:center}.content-box{width:fit-content;float:left}\
.ctrl{margin-left:15px;margin-top:15px;margin-right:50px}.temp{font-size:72px}\
.arrow{font-size:25px}button{margin:5px}</style>\
<h1 class=\"pd border center\">Smart Thermostat Control Panel</h1>\
<div class=\"center flx\">\
<div class=\"pd mr border content-box\">\
<div class=center>Thermostat Controls</div>\
<div class=flx>\
<div class=\"temp ctrl\">%d&deg;%c</div>\
<div class=\"pd mr\"><button onclick='window.location.href=\"/tempUp\"'>&uarr;\
</button><br>\
<button onclick='window.location.href=\"/tempDown\"'>&darr;</button>\
</div>\
</div>\
<br>Set to %s [Current action: %s]<br>\
<button onclick='window.location.href=\"/hvacModeOff\"'>OFF</button>\
<button onclick='window.location.href=\"/hvacModeAuto\"'>AUTO</button>\
<button onclick='window.location.href=\"/hvacModeHeat\"'>HEAT</button>\
<button onclick='window.location.href=\"/hvacModeCool\"'>COOL</button>\
<button onclick='window.location.href=\"/hvacModeFan\"'>FAN</button>\
</div>\
<div class=\"pd mr border content-box\">Current Readings<div class=\"pd mr temp\">%0.1f&deg;%c\
</div>%0.1f%% humidity<br>Light Level: %d<br>Motion: %s</div>\
<div class=\"pd mr border content-box\">Thermostat Settings\
<br><br><br><br>Units: %c<br>Swing: %0.1f<br>Temp. Correction: %0.1f\
</div>\
<div class=\"pd mr border content-box\">System Settings<br>\
<br>WiFi Strength: %d%%<br>IP: %s<br><br>Firmware Version %s<br>%s<br>\
<button onclick='window.location.href=\"/upload\"'>Update Firmware</button>\
<br><button onclick='window.location.href=\"/clearFirmware\"'>Clear Config</button><br>\
</div>\
</div><br><br><br>\
<footer>%s<br>\
<a href=https://github.com/smeisner/smart-thermostat target=_blank>Smart Thermostat Project on GitHub</a>\
</footer>";

const char *web_fw_upload = "\
<!DOCTYPE html>\
<html>\
	<head>\
		<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" />\
		<title>ESP32 OTA Update</title>\
		<script>\
			function startUpload() {\
				var otafile = document.getElementById(\"otafile\").files;\
				if (otafile.length == 0) {\
					alert(\"No file selected!\");\
				} else {\
					document.getElementById(\"otafile\").disabled = true;\
					document.getElementById(\"upload\").disabled = true;\
					var file = otafile[0];\
					var xhr = new XMLHttpRequest();\
					xhr.onreadystatechange = function() {\
						if (xhr.readyState == 4) {\
							if (xhr.status == 200) {\
								document.open();\
								document.write(xhr.responseText);\
								document.close();\
							} else if (xhr.status == 0) {\
								alert(\"Server closed the connection abruptly!\");\
								location.reload()\
							} else {\
								alert(xhr.status + \" Error!\\n\" + xhr.responseText);\
								location.reload()\
							}\
						}\
					};\
					xhr.upload.onprogress = function (e) {\
						var progress = document.getElementById(\"progress\");\
						progress.textContent = \"Progress: \" + (e.loaded / e.total * 100).toFixed(0) + \"%\";\
					};\
					xhr.open(\"POST\", \"/update\", true);\
					xhr.send(file);\
				}\
			}\
		</script>\
	</head>\
	<body>\
		<h1>ESP32 OTA Firmware Update</h1>\
		<div>\
			<label for=\"otafile\">Firmware file:</label>\
			<input type=\"file\" id=\"otafile\" name=\"otafile\" />\
		</div>\
		<div>\
			<button id=\"upload\" type=\"button\" onclick=\"startUpload()\">Upload</button>\
		</div>\
		<div id=\"progress\"></div>\
	</body>\
</html>";
