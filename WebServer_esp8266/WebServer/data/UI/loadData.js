const serverIp="http://192.168.0.9";

const deviceContainer=document.querySelector(".container2");
deviceContainer.innerHTML="";
displayDevices();
LoadMapFromServer();
setInterval(displayDevices, 1000);


function displayDevices(){
  
    const placedDevices=Array.from(document.querySelectorAll(".recieverID")).map(paragraph => paragraph.textContent);
    
    fetch(serverIp+"/distances")
  .then(response => {
    if (!response.ok) {
      throw new Error(`HTTP error! Status: ${response.status}`);
    }
    return response.json();
  })
  .then(data => {
    
    
    if (data.length===0 && placedDevices.length===0) {
      deviceContainer.innerHTML=`<h2 class="nodevices" style="color:white; margin-top:0.5em">No devices!</h2>`;
      return;
    }
    else if( deviceContainer.querySelector(`h2.nodevices`)){
      deviceContainer.querySelector(`h2.nodevices`).remove();
    }
    
    data.sort((a,b)=>a.id-b.id);
    console.log("Data received:", data);    
    const extractedIds=data.map((device)=>device.id);

    data.forEach(device => {
      const currentDiv=document.querySelector(`div[id="${device.id}"]`);
      if (currentDiv) {
        if (currentDiv.querySelector(".offline")) {
          currentDiv.classList.remove("offline");
          currentDiv.querySelector(".offline").textContent="Connected";
          currentDiv.querySelector(".offline").classList="online";
        }
        
        currentDiv.querySelector(".distance").innerHTML=`<span>Distance:</span>${device.distance=="-1"?"--":device.distance}m`;
        currentDiv.querySelector(".rssi").innerHTML=`<span>RSSI:</span>${device.avgRSSI=="-1"?"--":device.avgRSSI}db`;
        currentDiv.querySelector(".signalstr").innerHTML=`RSSI at 1m: ${device.rssi1m}db`;
      }
      else
      deviceContainer.innerHTML+=`<div class="devicecontainer" id="${device.id}" onmouseenter="AddOutline(this)" onmouseleave="RemoveOutline(this)">
                                    <h2>${device.id}</h2>
                                    <p class="online">Connected</p>
                                    <div class="distanceAndRSSI">
                                      <p class="distance"><span>Distance:</span>${device.distance=="-1"?"--":device.distance}m</p>
                                      <p class="rssi"><span>RSSI:</span>${device.avgRSSI=="-1"?"--":device.avgRSSI}db</p>
                                    </div>           
                                    <p class="signalstr">RSSI at 1m: ${device.rssi1m}db</p>
                                    <a class="calibrate-button" onclick="InitiateCalibration(this)">Calibrate</a>
                                    <p class="type">Type: ${device.type}</p>
                                  </div>`
    });

    
    placedDevices.forEach((x)=>{
      const currentDiv=document.querySelector(`div[id="${x}"]`);
      if(!extractedIds.includes(x))
      if (currentDiv&& !currentDiv.querySelector("p.offline")) {
        currentDiv.classList+=" offline";
        currentDiv.querySelector(".distance").innerHTML=`<span>Distance:</span>--`;
        currentDiv.querySelector(".rssi").innerHTML=`<span>RSSI:</span>--`;
        currentDiv.querySelector(".signalstr").innerHTML=`RSSI at 1m: --`;
        currentDiv.querySelector(".online").textContent="Disconnected";
        currentDiv.querySelector(".online").classList="offline";
      }
      if(!currentDiv&& currentDiv.querySelector("p.offline"))
        deviceContainer.innerHTML+=`<div class="devicecontainer offline" id="${x}" onmouseenter="AddOutline(this)" onmouseleave="RemoveOutline(this)">
                                      <h2>${x}</h2>
                                      <p class="offline">Disconnected</p>
                                      <div class="distanceAndRSSI">
                                        <p class="distance"><span>Distance:</span> --</p>
                                        <p class="rssi"><span>RSSI:</span>--</p>
                                      </div>           
                                      <p class="signalstr">RSSI at 1m: --</p>
                                      <a class="calibrate-button" onclick="InitiateCalibration(this)">Calibrate</a>                                
                                      <p class="type">Type: ESP32</p>
                                    </div>`
      
    });

    const currentDevices=document.querySelectorAll(".devicecontainer");
    currentDevices.forEach(element => {
      if(!placedDevices.includes(element.querySelector("h2").textContent)&&!extractedIds.includes(element.querySelector("h2").textContent))
      element.remove();
    });

    data.forEach((el)=>{
      if (Number(el.distance)!==-1) 
      DrawDistances(el.id,Number(el.distance));
    })
  });
}

function InitiateCalibration(e){
  const deviceId=e.parentElement.id;
  fetch(serverIp+"/calibrationStatus", {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({"Id":deviceId}), 
  })
    .then(response => {  
      if (response.status===200) {
        
        e.outerHTML=`<div class="accept-cancel-container">
                      <a class="accept-btn" onclick="StartCalibration(this)">Start</a>
                      <a class="cancel-btn" onclick="CancelCalibration(this)">Cancel</a>
                    </div>`;
                    
                   
        Array.from(document.querySelectorAll(`div.devicecontainer[id="${deviceId}"] > div.accept-cancel-container > a`))
              .forEach((x)=>{x.style.maxHeight = "0";x.style.padding = "0"; x.style.transition="max-height 2s,padding 0.4s";}); 

              setTimeout(() => {
                Array.from(document.querySelectorAll(`div.devicecontainer[id="${deviceId}"] > div.accept-cancel-container > a`))
                .forEach((element) => {
                  element.style.maxHeight = "500px";
                  element.style.padding= "0.1em 0.5em";
                });
              }, 10);
      }
    })
    .catch(error => {
      console.error('Error:', error);
    });
}
function StartCalibration(e) {
  const deviceId=e.parentElement.parentElement.id;

  fetch(serverIp+"/startCalibration", {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({"Id":deviceId}), 
  })
    .then(response => {  
      if (response.status===200) {
        e.parentElement.outerHTML=`<a class="calibrate-button" onclick="InitiateCalibration(this)">Calibrate</a>`;
      }
    })
    .catch(error => {
      console.error('Error:', error);
    });
}
function CancelCalibration(e) {
  const deviceId=e.parentElement.parentElement.id;

  fetch(serverIp+"/stopCalibration", {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({"Id":deviceId}), 
  })
    .then(response => {  
      if (response.status===200) {
        e.parentElement.outerHTML=`<a class="calibrate-button" onclick="InitiateCalibration(this)">Calibrate</a>`;
      }
    })
    .catch(error => {
      console.error('Error:', error);
    });
}

function LoadMapFromServer() {
  fetch(serverIp+'/map')
  .then((r)=>{
    if (r.status!=200) {
      throw new Error(`HTTP error! Status: ${r.status}`);
    }
    return r.json();
  })
    .then((obj)=>{

    document.querySelector(".map").innerHTML =
      '<div class="mapcontrols"> <div class="mapbtn" onclick="zoom()">+</div> <div class="mapbtn move"> <svg xmlns="http://www.w3.org/2000/svg" height="1em" viewBox="0 0 512 512" > <!--! Font Awesome Free 6.4.2 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --> <style> svg { fill: #ffffff; } </style> <path d="M278.6 9.4c-12.5-12.5-32.8-12.5-45.3 0l-64 64c-9.2 9.2-11.9 22.9-6.9 34.9s16.6 19.8 29.6 19.8h32v96H128V192c0-12.9-7.8-24.6-19.8-29.6s-25.7-2.2-34.9 6.9l-64 64c-12.5 12.5-12.5 32.8 0 45.3l64 64c9.2 9.2 22.9 11.9 34.9 6.9s19.8-16.6 19.8-29.6V288h96v96H192c-12.9 0-24.6 7.8-29.6 19.8s-2.2 25.7 6.9 34.9l64 64c12.5 12.5 32.8 12.5 45.3 0l64-64c9.2-9.2 11.9-22.9 6.9-34.9s-16.6-19.8-29.6-19.8H288V288h96v32c0 12.9 7.8 24.6 19.8 29.6s25.7 2.2 34.9-6.9l64-64c12.5-12.5 12.5-32.8 0-45.3l-64-64c-9.2-9.2-22.9-11.9-34.9-6.9s-19.8 16.6-19.8 29.6v32H288V128h32c12.9 0 24.6-7.8 29.6-19.8s2.2-25.7-6.9-34.9l-64-64z" /> </svg> </div> <div class="mapbtn" onclick="zoomOut()">-</div> </div>';
    document.querySelector(".move").addEventListener("mousedown", Hold);
    document.addEventListener("mouseup", () => {
      cancelAnimationFrame(animationFrameId);
    });
    obj.forEach((obj) => {
      const div = createElementFromHTML(obj);
      div.addEventListener("mousedown", Hold);
      document.querySelector(".map").appendChild(div);
    });

    ExitEditor();
  })
}

function DrawDistances(id, distance) {
 const objReciver= Array.from(document.querySelectorAll(".recieverID"))
 .find((element)=>element.textContent===id)
 .parentElement;

 var initialRadius=Number(window.getComputedStyle(objReciver).width.split("p")[0])/2;
 console.log(initialRadius);

 var distanceInPx=Mtopx(distance);
 objReciver.style.scale=distanceInPx/initialRadius;
}