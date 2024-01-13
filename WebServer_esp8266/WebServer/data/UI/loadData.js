const serverIp="http://192.168.0.9";

const deviceContainer=document.querySelector(".container2");
deviceContainer.innerHTML="";
displayDevices();
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
      deviceContainer.innerHTML+=`<div class="devicecontainer" id="${device.id}">
                                    <h2>${device.id}</h2>
                                    <p class="online">Connected</p>
                                    <div class="distanceAndRSSI">
                                      <p class="distance"><span>Distance:</span>${device.distance=="-1"?"--":device.distance}m</p>
                                      <p class="rssi"><span>RSSI:</span>${device.avgRSSI=="-1"?"--":device.avgRSSI}db</p>
                                    </div>           
                                    <p class="signalstr">RSSI at 1m: ${device.rssi1m}db</p>
                                    <a class="calibrate-button" onclick="InitiateScan(this)">Calibrate</a>
                                    <p class="type">Type: ${device.type}</p>
                                  </div>`
    });

    
    placedDevices.forEach((x)=>{
      const currentDiv=document.querySelector(`div[id="${x}"]`);
      if (!extractedIds.includes(x) && !currentDiv.querySelector("p.offline")) {
      if (currentDiv) {
        currentDiv.classList+=" offline";
        currentDiv.querySelector(".distance").innerHTML=`<span>Distance:</span>--`;
        currentDiv.querySelector(".rssi").innerHTML=`<span>RSSI:</span>--`;
        currentDiv.querySelector(".signalstr").innerHTML=`RSSI at 1m: --`;
        currentDiv.querySelector(".online").textContent="Disconnected";
        currentDiv.querySelector(".online").classList="offline";
      }
      else
        deviceContainer.innerHTML+=`<div class="devicecontainer offline" id="${x}">
                                      <h2>${x}</h2>
                                      <p class="offline">Disconnected</p>
                                      <div class="distanceAndRSSI">
                                        <p class="distance"><span>Distance:</span> --</p>
                                        <p class="rssi"><span>RSSI:</span>--</p>
                                      </div>           
                                      <p class="signalstr">RSSI at 1m: -55db</p>
                                      <a class="calibrate-button" onclick="InitiateScan()">Calibrate</a>                                
                                      <p class="type">Type: ESP32</p>
                                    </div>`
      }
    });

    const currentDevices=document.querySelectorAll(".devicecontainer");
    currentDevices.forEach(element => {
      if(!placedDevices.includes(element.querySelector("h2").textContent)&&!extractedIds.includes(element.querySelector("h2").textContent))
      element.remove();
    });
  });
}

function InitiateScan(e){
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
        e.parentElement.outerHTML=`<a class="calibrate-button" onclick="InitiateScan(this)">Calibrate</a>`;
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
        e.parentElement.outerHTML=`<a class="calibrate-button" onclick="InitiateScan(this)">Calibrate</a>`;
      }
    })
    .catch(error => {
      console.error('Error:', error);
    });
}