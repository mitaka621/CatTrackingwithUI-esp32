const serverIp = "http://192.168.0.9";

const deviceContainer = document.querySelector(".container2");
deviceContainer.innerHTML = "";
displayDevices();
LoadMapFromServer();

document.querySelector(".container").innerHTML = "";
displayNotifications();

function formatDateTime(isoString) {
  const date = new Date(isoString);
  const hours = String(date.getHours()).padStart(2, '0');
  const minutes = String(date.getMinutes()).padStart(2, '0');
  const seconds = String(date.getSeconds()).padStart(2, '0');
  const day = String(date.getDate()).padStart(2, '0');
  const month = String(date.getMonth() + 1).padStart(2, '0');
  const year = date.getFullYear();

  return `${hours}:${minutes}:${seconds} - ${day}.${month}.${year}`;
}

async function createNotification(level, title, message, dateTimeNotif) {
  const notificationContainer = document.createElement('div');
  notificationContainer.className = `notificationcontainer level${level}`;

  const deleteLink = document.createElement('a');
  deleteLink.setAttribute("onclick", "deleteNotification(this)");

  const notifLevelBanner = document.createElement('div');
  const pLevel = document.createElement('p');
  switch (level) {
    case 1:
      notifLevelBanner.className = 'notification-level';
      pLevel.textContent = "Information";
      break;
    case 2:
      notifLevelBanner.className = 'notification-level';
      pLevel.textContent = "Warning";
      break;
    case 3:
      notifLevelBanner.className = 'notification-level-reverse';
      pLevel.textContent = "Danger Needs Attention";
      break;
  }

  notifLevelBanner.appendChild(pLevel);

  const deleteIcon = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  deleteIcon.setAttribute('height', '1em');
  deleteIcon.setAttribute('viewBox', '0 0 448 512');
  deleteIcon.innerHTML = `
    <path d="M135.2 17.7L128 32H32C14.3 32 0 46.3 0 64S14.3 96 32 96H416c17.7 0 32-14.3 32-32s-14.3-32-32-32H320l-7.2-14.3C307.4 6.8 296.3 0 284.2 0H163.8c-12.1 0-23.2 6.8-28.6 17.7zM416 128H32L53.2 467c1.6 25.3 22.6 45 47.9 45H346.9c25.3 0 46.3-19.7 47.9-45L416 128z" />
  `;
  deleteLink.appendChild(deleteIcon);
  notificationContainer.appendChild(deleteLink);

  notificationContainer.appendChild(notifLevelBanner);

  const titleElement = document.createElement('h2');
  titleElement.textContent = title;
  notificationContainer.appendChild(titleElement);

  const messageElement = document.createElement('p');
  messageElement.className = 'message';
  messageElement.textContent = message;
  notificationContainer.appendChild(messageElement);

  const dateTime = document.createElement('p');
  dateTime.className = 'date-time';
  if (dateTimeNotif) {
    dateTime.textContent = formatDateTime(dateTimeNotif);
  } else {
    dateTime.textContent = 'Unknown';
  }
  notificationContainer.appendChild(dateTime);

  if (document.getElementById("no-notifications-alert")) {
    document.getElementById("no-notifications-alert").remove();
  }

  notificationContainer.style.maxHeight = 0;
  notificationContainer.style.maxWidth = 0;
  notificationContainer.style.marginTop = 0;
  notificationContainer.style.marginBottom = 0;

  if (document.querySelector(".container").childElementCount != 0) {
    document.querySelector(".container").insertBefore(notificationContainer, document.querySelector(".container").firstChild);
  } else {
    document.querySelector(".container").appendChild(notificationContainer);
  }

  await setTimeout(() => {
    notificationContainer.style.marginTop = '1em';
    notificationContainer.style.marginBottom = '1em';
    notificationContainer.style.maxHeight = '999px';
    notificationContainer.style.maxWidth = '100%';
  }, 100);
}

function displayNotifications() {
  fetch("/notifications")
    .then(r => r.json())
    .then(j => {
      j.forEach(notification => {
        createNotification(notification.notificationLevel, notification.title, notification.description, notification.dateTime);
      })
    });
}

function displayOnlyLatestNotification() {
  fetch("/newNotification")
    .then(r => r.json())
    .then(j => createNotification(j.notificationLevel, j.title, j.description, j.dateTime));
}

async function displayDevices() {

  const placedDevices = Array.from(document.querySelectorAll(".recieverID")).map(paragraph => paragraph.textContent);

  await fetch(serverIp + "/distances", {
    signal: AbortSignal.timeout(2000)
  })
    .then(response => {
      if (!response.ok) {
        throw new Error(`HTTP error! Status: ${response.status}`);
      }
      return response.json();
    })
    .then(json => {

      if (json.newNotifications) {
        displayOnlyLatestNotification()();
      }

      const data = json.data;
      if (data.length === 0 && placedDevices.length === 0) {
        deviceContainer.innerHTML = `<h2 class="nodevices" style="color:white; margin-top:0.5em">No devices!</h2>`;
        return;
      }
      else if (deviceContainer.querySelector(`h2.nodevices`)) {
        deviceContainer.querySelector(`h2.nodevices`).remove();
      }

      data.sort((a, b) => b.avgRSSI - a.avgRSSI);
      const extractedIds = data.map((device) => device.id);

      data.forEach(device => {
        const currentDiv = document.querySelector(`div[id="${device.id}"]`);
        if (currentDiv) {
          if (device.isConnected) {
            currentDiv.classList.remove("offline");
            if (currentDiv.querySelector(".offline")) {
              currentDiv.querySelector(".offline").textContent = "Connected";
              currentDiv.querySelector(".offline").classList = "online";
            }

            currentDiv.querySelector(".distance").innerHTML = `<span>Distance:</span>${device.distance == "-1" ? "--" : device.distance}m`;
            currentDiv.querySelector(".rssi").innerHTML = `<span>RSSI:</span>${device.avgRSSI == "-1" ? "--" : device.avgRSSI}db`;
            currentDiv.querySelector(".signalstr").innerHTML = `RSSI at 1m: ${device.rssi1m}db`;

          }
          else {
            currentDiv.classList += " offline";
            currentDiv.querySelector(".distance").innerHTML = `<span>Distance:</span>--`;
            currentDiv.querySelector(".rssi").innerHTML = `<span>RSSI:</span>--`;
            currentDiv.querySelector(".signalstr").innerHTML = `RSSI at 1m: --`;
            if (currentDiv.querySelector(".online")) {
              currentDiv.querySelector(".online").textContent = "Disconnected";
              currentDiv.querySelector(".online").classList = "offline";
            }
          }
        }
        else {
          if (device.isConnected) {
            deviceContainer.innerHTML += `<div class="devicecontainer" id="${device.id}" onmouseenter="AddOutline(this)" onmouseleave="RemoveOutline(this)">
                                    <h2>${device.id}</h2>
                                    <p class="online">Connected</p>
                                    <div class="distanceAndRSSI">
                                      <p class="distance"><span>Distance:</span>${device.distance == "-1" ? "--" : device.distance}m</p>
                                      <p class="rssi"><span>RSSI:</span>${device.avgRSSI == "-1" ? "--" : device.avgRSSI}db</p>
                                    </div>           
                                    <p class="signalstr">RSSI at 1m: ${device.rssi1m}db</p>
                                    <a id="${device.localIp}" class="calibrate-button" onclick="InitiateCalibration(this)">Calibrate</a>
                                    <p class="type">Type: ${device.type}</p>
                                  </div>`
          }
          else {
            deviceContainer.innerHTML += `<div class="devicecontainer offline" id="${device.id}" onmouseenter="AddOutline(this)" onmouseleave="RemoveOutline(this)">
                                            <h2>${device.id}</h2>
                                            <p class="offline">Disconnected</p>
                                            <div class="distanceAndRSSI">
                                              <p class="distance"><span>Distance:</span> --</p>
                                              <p class="rssi"><span>RSSI:</span>--</p>
                                            </div>           
                                            <p class="signalstr">RSSI at 1m: --</p>
                                            <a id="${device.localIp}" class="calibrate-button" onclick="InitiateCalibration(this)">Calibrate</a>                                
                                            <p class="type">Type: ESP32</p>
                                          </div>`
          }
        }
      });

      const currentDevices = document.querySelectorAll(".devicecontainer");
      currentDevices.forEach(element => {
        if (!placedDevices.includes(element.querySelector("h2").textContent) && !extractedIds.includes(element.querySelector("h2").textContent))
          element.remove();
      });

      let points = [];
      let count = 0;

      data.forEach((el) => {
        if (Number(el.distance) !== -1 && points.length < 3) {
          clearAllDistance();
          try {
            const objReciver = Array.from(document.querySelectorAll(".recieverID"))
              .find((element) => element.textContent === el.id)
              .parentElement;

            if (count === 0) {
              if (objReciver.parentElement.classList && objReciver.parentElement.classList.contains("room") && objReciver.parentElement.querySelector("input").value) {
                document.getElementById("location-name").textContent = objReciver.parentElement.querySelector("input").value;
              } else {
                document.getElementById("location-name").textContent = "Near " + el.id;
              }

              if (el.distance <= 2) {
                clearAllDistance();
                clearCanvas();
                DrawDistances(el.id, Number(el.distance));

                count++;
                return;
              }
            }
            count++;

            const top = Number(objReciver.style.top.split("p")[0]);
            const left = Number(objReciver.style.left.split("p")[0]);

            let middleTop = top + (Number(getComputedStyle(objReciver).height.split("p")[0]) / 2.0);
            let middleLeft = left + (Number(getComputedStyle(objReciver).width.split("p")[0]) / 2.0);


            if (objReciver.parentElement.classList && objReciver.parentElement.classList.contains("room")) {
              middleTop += Number(getComputedStyle(objReciver.parentElement).top.split("p")[0]);
              middleLeft += Number(getComputedStyle(objReciver.parentElement).left.split("p")[0]);
            }

            if (objReciver) {
              points.push({ y: middleTop, x: middleLeft })
            }
          }
          catch { }
        }
      });

      let isFound = false;
      if (points.length < 3) {
        for (let index = 0; index < data.length; index++) {
          const el = data[index];
          if (Number(el.distance) !== -1 && Array.from(document.querySelectorAll(".recieverID"))
            .find((element) => element.textContent === el.id)) {
            clearAllDistance();
            clearCanvas();
            DrawDistances(el.id, Number(el.distance));
            isFound = true;
            break;
          }
        }

        if (!isFound) {
          clearAllDistance();
          clearCanvas();
          document.getElementById("location-name").textContent = "CONNECTION LOST";
          return;
        }

      }

      drawTriangle(points);
    }).catch(e => console.log(e));

  await setTimeout(function () {
    displayDevices();
  }, 1000);
}

function InitiateCalibration(e) {
  const deviceId = e.parentElement.id;
  const ip = e.id;
  fetch(`http://${ip}/blinkOn`)
    .then(response => {
      if (response.status === 200) {

        e.outerHTML = `<div class="accept-cancel-container">
                        <p>Place the BLE beacon exactly 1m away from the device and click start.</p>
                        <a id=${ip} class="accept-btn" onclick="StartCalibration(this)">Start</a>
                        <a id=${ip} class="cancel-btn" onclick="CancelCalibration(this)">Cancel</a>
                      </div>`;


        Array.from(document.querySelectorAll(`div.devicecontainer[id="${deviceId}"] > div.accept-cancel-container > a`))
          .forEach((x) => { x.style.maxHeight = "0"; x.style.padding = "0"; x.style.transition = "max-height 2s,padding 0.4s"; });

        setTimeout(() => {
          Array.from(document.querySelectorAll(`div.devicecontainer[id="${deviceId}"] > div.accept-cancel-container > a`))
            .forEach((element) => {
              element.style.maxHeight = "500px";
              element.style.padding = "0.1em 0.5em";
            });
        }, 10);
      }
    })
    .catch(error => {
      console.error('Error:', error);
    });
}
function StartCalibration(e) {
  const ip = e.id;

  e.parentElement.querySelector(".cancel-btn").remove();
  e.parentElement.querySelector(".accept-btn").textContent = "Calibrating ........";
  e.parentElement.querySelector(".accept-btn").setAttribute("onclick", "");
  fetch(`http://${ip}/beginCalibration`)
    .then(response => {
      if (response.status === 200) {
        e.parentElement.outerHTML = `<a id="${ip}" class="calibrate-button" onclick="InitiateCalibration(this)">Calibrate</a>`;
      }
    })
    .catch(error => {
      console.error('Error:', error);
    });
}
function CancelCalibration(e) {
  const ip = e.id;

  e.parentElement.querySelector(".accept-btn").remove();
  e.parentElement.querySelector(".cancel-btn").textContent = "Canceling ........";
  e.parentElement.querySelector(".cancel-btn").setAttribute("onclick", "");

  fetch(`http://${ip}/blinkOff`)
    .then(response => {
      if (response.status === 200) {
        e.parentElement.outerHTML = `<a id="${ip}" class="calibrate-button" onclick="InitiateCalibration(this)">Calibrate</a>`;
      }
    })
    .catch(error => {
      console.error('Error:', error);
    });
}

function LoadMapFromServer() {
  fetch(serverIp + '/UI/resources/map.json')
    .then((r) => {
      if (r.status != 200) {
        throw new Error(`HTTP error! Status: ${r.status}`);
      }
      return r.json();
    })
    .then((obj) => {

      document.querySelector(".map").innerHTML =
        '<canvas id="position-canvas" class="position-display"></canvas><div class="mapcontrols"> <div class="mapbtn" onclick="zoom()">+</div> <div class="mapbtn move"> <svg xmlns="http://www.w3.org/2000/svg" height="1em" viewBox="0 0 512 512" > <!--! Font Awesome Free 6.4.2 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --> <style> svg { fill: #ffffff; } </style> <path d="M278.6 9.4c-12.5-12.5-32.8-12.5-45.3 0l-64 64c-9.2 9.2-11.9 22.9-6.9 34.9s16.6 19.8 29.6 19.8h32v96H128V192c0-12.9-7.8-24.6-19.8-29.6s-25.7-2.2-34.9 6.9l-64 64c-12.5 12.5-12.5 32.8 0 45.3l64 64c9.2 9.2 22.9 11.9 34.9 6.9s19.8-16.6 19.8-29.6V288h96v96H192c-12.9 0-24.6 7.8-29.6 19.8s-2.2 25.7 6.9 34.9l64 64c12.5 12.5 32.8 12.5 45.3 0l64-64c9.2-9.2 11.9-22.9 6.9-34.9s-16.6-19.8-29.6-19.8H288V288h96v32c0 12.9 7.8 24.6 19.8 29.6s25.7 2.2 34.9-6.9l64-64c12.5-12.5 12.5-32.8 0-45.3l-64-64c-9.2-9.2-22.9-11.9-34.9-6.9s-19.8 16.6-19.8 29.6v32H288V128h32c12.9 0 24.6-7.8 29.6-19.8s2.2-25.7-6.9-34.9l-64-64z" /> </svg> </div> <div class="mapbtn" onclick="zoomOut()">-</div> </div>';
      document.querySelector(".move").addEventListener("mousedown", Hold);
      document.addEventListener("mouseup", () => {
        cancelAnimationFrame(animationFrameId);
      });
      obj.forEach((obj) => {
        const div = createElementFromHTML(obj);
        div.addEventListener("mousedown", Hold);
        if (div.querySelector(".object")) {
          div.querySelectorAll(".object").forEach(item => {
            item.addEventListener("mousedown", Hold);
          })
        }
        document.querySelector(".map").appendChild(div);
      });

      resizeCanvas();
      ExitEditor();
    })
}

function DrawDistances(id, distance) {
  const objReciver = Array.from(document.querySelectorAll(".recieverID"))
    .find((element) => element.textContent === id)
    .parentElement;

  var initialRadius = Number(window.getComputedStyle(objReciver).width.split("p")[0]) / 2;


  objReciver.style.backgroundColor = "#ff8d8d62";

  if (distance < 1) {
    objReciver.style.scale = "initial";
    return;
  }

  var distanceInPx = Mtopx(distance);

  objReciver.style.scale = distanceInPx / initialRadius;
}

function clearAllDistance() {
  Array.from(document.querySelectorAll(".reciever")).forEach(r => {
    r.style.scale = "initial";
    r.style.removeProperty("background-color");
  })
}

let canvas;
let ctx;

function initCanvas() {
  canvas = document.getElementById('position-canvas');
  ctx = canvas.getContext('2d');
}



function clearCanvas() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);
}

function drawTriangle(arr) {
  initCanvas();
  clearCanvas();
  resizeCanvas();

  if (arr.length === 0) {
    return;
  }

  ctx.beginPath();
  ctx.moveTo(arr[0].x, arr[0].y);

  for (let i = 1; i < arr.length; i++) {
    ctx.lineTo(arr[i].x, arr[i].y);
  }

  ctx.closePath();

  // Set the fill and stroke styles to red
  ctx.fillStyle = '#ff8d8d62';

  // Fill and stroke the triangle
  ctx.fill();
}

function resizeCanvas() {
  const container = document.querySelector('.map');
  canvas.setAttribute("width", getComputedStyle(container).width);
  canvas.setAttribute("height", getComputedStyle(container).height);

}

initCanvas();
resizeCanvas();