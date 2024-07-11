const backgroundContainer = document.querySelector('.background-container');

// Function to generate a random integer between min and max
function getRandomInt(min, max) {
  return Math.floor(Math.random() * (max - min + 1)) + min;
}

// Function to generate a random polygon shape
function generatePolygonPoints() {
  let points = '';
  for (let i = 0; i < 5; i++) {
    const x = getRandomInt(0, 100);
    const y = getRandomInt(0, 100);
    points += `${x}% ${y}%, `;
  }
  return points.slice(0, -2);
}

// Generate multiple shapes and append them to the background container
for (let i = 0; i < 50; i++) {
  const shape = document.createElement('div');
  shape.classList.add('shape');
  shape.style.left = `${getRandomInt(0, window.innerWidth)}px`;
  shape.style.top = `${getRandomInt(10, Number(getComputedStyle(document.querySelector(".background-container")).height.split("p")[0]))}px`; // Spawning above the top of the page
  shape.style.animationDuration = `${getRandomInt(10, 20)}s`;
  shape.style.width = `${getRandomInt(10, 30)}px`;
  shape.style.height = `${getRandomInt(10, 30)}px`;
  shape.style.clipPath = `polygon(${generatePolygonPoints()})`;
  backgroundContainer.appendChild(shape);
}
document.addEventListener('DOMContentLoaded', () => {


  const title = document.querySelector('.title');

  setTimeout(() => {
    title.style.top = "-100%";
    document.querySelectorAll(".shape").forEach(x => x.style.top = '2000px');

  }, 3000);

  setTimeout(() => {
    document.querySelector(".background-container").style.opacity = 0;
    document.querySelector("header").style.display = "none";
  }, 4000);
  setTimeout(() => document.querySelector(".background-container").style.display = "none", 5000);
});

let timeoutId;

let oldCursorPosX = 0,
  oldCursorPosY = 0,
  offsetX,
  offsetY;



function detectMouseMovement(event) {
  offsetX = (event.clientX - oldCursorPosX) * 0.7;
  offsetY = (event.clientY - oldCursorPosY) * 0.7;
  oldCursorPosX = event.clientX;
  oldCursorPosY = event.clientY;
  clearTimeout(timeoutId);

  timeoutId = setTimeout(function () {
    offsetX = 0;
    offsetY = 0;
  }, 50);
}

window.addEventListener("load", AddEventListeners);

function AddEventListeners() {
  document.addEventListener("mousemove", detectMouseMovement);
  document.querySelector(".button").addEventListener("click", EditMap);
}

let zoomLevel = 1;
function pxToM(px, modifier = 0.015) {
  return (
    (px * modifier) /
    (zoomLevel < 0 ? 1 / (zoomLevel * -1) : zoomLevel)
  ).toFixed(1);
}

function Mtopx(px, modifier = 0.015) {
  return (
    (px / modifier) *
    (zoomLevel < 0 ? 1 / (zoomLevel * -1) : zoomLevel)
  ).toFixed(1);
}

function EditMap() {
  const btn = document.querySelector(".button");

  document.querySelector(".button2.green").style.maxHeight = "0";
  document.querySelector(".button2.green").style.padding = "0";
  btn.style.maxWidth = "0";
  document.querySelector(".container").style.height = "0";
  document.querySelector(".container2").style.height = "50%";
  document.querySelector(".container3").style.width = "100%";
  document.querySelector(".container3").style.height = "50%";
  document.querySelectorAll(".button2")[1].removeEventListener("click", unwrap);

  setTimeout(() => {
    btn.querySelector("p").innerHTML =
      '<p><svg xmlns="http://www.w3.org/2000/svg" height="1em" viewBox="0 0 512 512"><!--! Font Awesome Free 6.4.2 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --><style>svg{fill:#ffffff}</style><path d="M177.5 414c-8.8 3.8-19 2-26-4.6l-144-136C2.7 268.9 0 262.6 0 256s2.7-12.9 7.5-17.4l144-136c7-6.6 17.2-8.4 26-4.6s14.5 12.5 14.5 22l0 72 288 0c17.7 0 32 14.3 32 32l0 64c0 17.7-14.3 32-32 32l-288 0 0 72c0 9.6-5.7 18.2-14.5 22z"/></svg>Close Editor<svg xmlns="http://www.w3.org/2000/svg" height="1em" viewBox="0 0 512 512"><!--! Font Awesome Free 6.4.2 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --><style>svg{fill:#ffffff}</style><path d="M177.5 414c-8.8 3.8-19 2-26-4.6l-144-136C2.7 268.9 0 262.6 0 256s2.7-12.9 7.5-17.4l144-136c7-6.6 17.2-8.4 26-4.6s14.5 12.5 14.5 22l0 72 288 0c17.7 0 32 14.3 32 32l0 64c0 17.7-14.3 32-32 32l-288 0 0 72c0 9.6-5.7 18.2-14.5 22z"/></svg></p>';

    btn.style.order = "5";
    btn.style.maxWidth = "40px";
  }, 500);

  const map = document.querySelector(".map");
  map.setAttribute("class", "map edit");

  btn.removeEventListener("click", EditMap);
  btn.addEventListener("click", ExitEditor);

  const placedDevices = Array.from(document.querySelectorAll(".recieverID"));
  document.querySelectorAll(".devicecontainer").forEach((item) => {

    if (
      !placedDevices.find((x) => x.textContent === item.children[0].textContent)
    ) {
      item.setAttribute("draggable", "true");
      item.setAttribute("ondragstart", "drag(event)");
      item.style.cursor = "";
    } else {
      item.classList.remove("offline");
      item.classList += " inactive";
    }
  });
  document.querySelectorAll(".recieverID").forEach((item) => {
    item.parentElement.addEventListener("mouseenter", AddOutline);
    item.parentElement.addEventListener("mouseleave", RemoveOutline);
    item.style.visibility = "visible";
  });
  document.querySelectorAll(".object").forEach((item) => {
    item.addEventListener("mousedown", Hold);
    if (!item.classList.contains("reciever")) {
      item.classList += " hover";

      item.querySelector(".measuring").style.visibility = item.offsetWidth > 60 ? "visible" : "hidden";

      item.querySelector(".height-measuring").style.visibility = item.offsetHeight > 60 ? "visible" : "hidden";
    }

    document.addEventListener("mouseup", () => {
      cancelAnimationFrame(animationFrameId);
    });
  });

  document.querySelector(".move").addEventListener("mousedown", Hold);
  document.addEventListener("mouseup", () => {
    cancelAnimationFrame(animationFrameId);
  });
}

function ExitEditor() {
  const mapJson = GenerateMapJsonString();
  fetch(serverIp + '/map', {
    method: 'POST',
    headers: {
      'Accept': 'application/json',
      'Content-Type': 'application/json',
      'Content-Length': mapJson.length
    },
    body: mapJson
  });

  const btn = document.querySelector(".button");
  btn.style.maxWidth = "0";
  document.querySelector(".container").style.height = "100%";
  document.querySelector(".button2.green").style.maxHeight = "41px";
  document.querySelector(".button2.green").style.paddingTop = "0.2em";
  document.querySelector(".button2.green").style.paddingBottom = "0.2em";
  document.querySelector(".container2").style.height = "0";
  document.querySelector(".container3").style.width = "0";
  document.querySelector(".container3").style.height = "0";
  document.querySelectorAll(".button2")[1].addEventListener("click", unwrap);

  setTimeout(() => {
    btn.style.order = "0";
    btn.querySelector("p").innerHTML =
      '<p> <svg xmlns="http://www.w3.org/2000/svg" height="1em" viewBox="0 0 512 512" > <!--! Font Awesome Free 6.4.2 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --> <style> svg { fill: #ffffff; } </style> <path d="M334.5 414c8.8 3.8 19 2 26-4.6l144-136c4.8-4.5 7.5-10.8 7.5-17.4s-2.7-12.9-7.5-17.4l-144-136c-7-6.6-17.2-8.4-26-4.6s-14.5 12.5-14.5 22l0 72L32 192c-17.7 0-32 14.3-32 32l0 64c0 17.7 14.3 32 32 32l288 0 0 72c0 9.6 5.7 18.2 14.5 22z" /> </svg> Edit Map <svg xmlns="http://www.w3.org/2000/svg" height="1em" viewBox="0 0 512 512" > <!--! Font Awesome Free 6.4.2 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --> <style> svg { fill: #ffffff; } </style> <path d="M334.5 414c8.8 3.8 19 2 26-4.6l144-136c4.8-4.5 7.5-10.8 7.5-17.4s-2.7-12.9-7.5-17.4l-144-136c-7-6.6-17.2-8.4-26-4.6s-14.5 12.5-14.5 22l0 72L32 192c-17.7 0-32 14.3-32 32l0 64c0 17.7 14.3 32 32 32l288 0 0 72c0 9.6 5.7 18.2 14.5 22z" /> </svg> </p>';

    btn.style.maxWidth = "41px";
  }, 300);
  document.querySelectorAll(".devicecontainer").forEach((item) => {

    if (item.children[1].classList.value === "offline") {
      item.classList = "devicecontainer offline";
    }
    else
      item.classList = "devicecontainer";
    item.style.cursor = "auto";
    item.setAttribute("draggable", "null");
    item.setAttribute("ondragstart", "null");
  });
  while (
    Array.from(
      document.querySelector(".sidemenu").querySelectorAll(".sidemenu>.button")
    ).some(() => true)
  ) {
    document
      .querySelector(".sidemenu")
      .removeChild(document.querySelector(".sidemenu>.button"));
  }

  const map = document.querySelector(".map");
  map.setAttribute("class", "map");

  document.querySelectorAll(".recieverID").forEach((item) => {
    item.style.visibility = "hidden";
  });

  document.querySelectorAll(".object").forEach((item) => {
    item.removeEventListener("mousedown", Hold);
    if (!item.classList.contains("reciever")) {
      item.classList.remove("hover")

      item.querySelector(".measuring").style.visibility = "hidden";

      item.querySelector(".height-measuring").style.visibility = "hidden";
    }
    document.removeEventListener("mouseup", () =>
      cancelAnimationFrame(animationFrameId)
    );
  });

  document.querySelector(".popup-menu").style.display = "none";

  document.querySelector(".button").removeEventListener("click", ExitEditor);
  document.querySelector(".button").addEventListener("click", EditMap);
}

let animationFrameId;

function Hold(e) {
  const obj = e.currentTarget;
  if (obj.classList == "mapbtn move") {
    moveMap();
  } else if (e.layerX > obj.offsetWidth - 6) {
    resizeObjectRight();
  } else if (e.layerX < 6) {
    resizeObjectLeft();
  } else if (e.layerY > obj.offsetHeight - 6) {
    resizeObjectBottom();
  } else if (e.layerY < 6) {
    resizeObjectTop();
  } else {
    moveObject();
  }

  function moveMap() {
    document.querySelectorAll("div.map>div.object").forEach((item) => {
      let newOffsetY = Math.round(
        Number(window.getComputedStyle(item).top.split("p")[0]) + offsetY * 2
      );
      let newOffsetX = Math.round(
        Number(window.getComputedStyle(item).left.split("p")[0]) + offsetX * 2
      );
      item.style.top = newOffsetY + "px";
      item.style.left = newOffsetX + "px";
    });
    animationFrameId = requestAnimationFrame(moveMap);
  }
  function moveObject() {
    let newOffsetY = Math.round(
      Number(window.getComputedStyle(obj).top.split("p")[0]) + offsetY
    );
    let newOffsetX = Math.round(
      Number(window.getComputedStyle(obj).left.split("p")[0]) + offsetX
    );

    obj.style.top = newOffsetY + "px";
    obj.style.left = newOffsetX + "px";
    animationFrameId = requestAnimationFrame(moveObject);
  }
  function resizeObjectBottom() {
    obj.style.height = Number(obj.offsetHeight + offsetY) + "px";
    if (obj.offsetHeight > 60) {
      obj.querySelector(".height-measuring").style.visibility = "visible";
      obj.querySelector(".measurement-height").textContent =
        pxToM(obj.offsetHeight) + "m";
    } else {
      obj.querySelector(".height-measuring").style.visibility = "hidden";
    }

    animationFrameId = requestAnimationFrame(resizeObjectBottom);
  }

  function resizeObjectTop() {
    let newOffsetY = Math.round(
      Number(window.getComputedStyle(obj).top.split("p")[0]) + offsetY
    );
    obj.querySelector(".measurement-height").textContent =
      pxToM(obj.offsetHeight) + "m";
    obj.style.top = newOffsetY + "px";
    obj.style.height = Number(obj.offsetHeight + offsetY * -1) + "px";
    animationFrameId = requestAnimationFrame(resizeObjectTop);

    if (obj.offsetHeight > 60) {
      obj.querySelector(".height-measuring").style.visibility = "visible";
    } else {
      obj.querySelector(".height-measuring").style.visibility = "hidden";
    }
  }
  function resizeObjectRight() {
    obj.style.width = Number(obj.offsetWidth + offsetX) + "px";

    obj.firstChild.querySelector(".measurement").textContent =
      pxToM(obj.offsetWidth) + "m";

    if (obj.offsetWidth > 60) {
      obj.querySelector(".measuring").style.visibility = "visible";
    } else {
      obj.querySelector(".measuring").style.visibility = "hidden";
    }

    animationFrameId = requestAnimationFrame(resizeObjectRight);
  }
  function resizeObjectLeft() {
    let newOffsetX = Math.round(
      Number(window.getComputedStyle(obj).left.split("p")[0]) + offsetX
    );
    obj.firstChild.querySelector(".measurement").textContent =
      pxToM(obj.offsetWidth) + "m";
    obj.style.left = newOffsetX + "px";
    obj.style.width = Number(obj.offsetWidth + offsetX * -1) + "px";
    animationFrameId = requestAnimationFrame(resizeObjectLeft);

    if (obj.offsetWidth > 60) {
      obj.querySelector(".measuring").style.visibility = "visible";
    } else {
      obj.querySelector(".measuring").style.visibility = "hidden";
    }
  }
}

function SnapToGrid() {
  const blocks = document.querySelectorAll(".map>.object");
  blocks.forEach((block) => {
    let posY = Number(window.getComputedStyle(block).top.split("p")[0]);
    let posX = Number(window.getComputedStyle(block).left.split("p")[0]);
    if (posY % 40 != 0) {
      if (posY % 40 < 20) {
        posY -= posY % 40;
      } else {
        posY += 40 - (posY % 40);
      }
    }

    if (posX % 40 != 0) {
      if (posX % 40 < 20) {
        posX -= posX % 40;
      } else {
        posX += 40 - (posX % 40);
      }
    }

    block.style.top = posY + "px";
    block.style.left = posX + "px";
  });
}

function allowDrop(ev) {
  ev.preventDefault();
}

function drag(ev) {
  ev.dataTransfer.setData("text", ev.target.children[0].textContent);
}
function drop(ev) {
  ev.preventDefault();

  const transferdData = ev.dataTransfer.getData("text");
  const deviceDiv = Array.from(
    document.querySelectorAll(".devicecontainer")
  ).find((x) => x.children[0].textContent === transferdData);
  deviceDiv.classList = "inactive devicecontainer";
  deviceDiv.removeAttribute("draggable");
  deviceDiv.removeAttribute("ondragstart");

  const newReciver = document.createElement("div");
  newReciver.addEventListener("mouseenter", AddOutline);
  newReciver.addEventListener("mouseleave", RemoveOutline);
  newReciver.setAttribute("onmouseenter", "AddOutline(this)");
  newReciver.setAttribute("onmouseleave", "RemoveOutline(this)");
  newReciver.classList += "object reciever";
  const p = document.createElement("p");
  p.classList += "recieverID";
  newReciver.appendChild(p);
  newReciver.children[0].textContent = transferdData;
  newReciver.style.top = ev.layerY + "px";
  newReciver.style.left = ev.layerX + "px";
  newReciver.addEventListener("mousedown", Hold);


  ev = ev.target;
  if (ev.classList.contains("reciever")) {
    ev = document.querySelector(".map");
  }
  if (ev.nodeName === "INPUT")
    ev = ev.parentElement.parentElement;

  ev.appendChild(newReciver);
}

let isEnabled = false;
function deleteBlocks(e) {
  if (!isEnabled) {
    document.querySelector(".mapcontrols").style.visibility = "hidden";
    isEnabled = true;
    e.classList += " delete";
    e.style.color = "#ffe205";
    document.querySelectorAll('[class="button"]').forEach((item) => {
      item.style.pointerEvents = "none";
      item.style.color = "grey";
    });
    document.querySelectorAll(".devicecontainer").forEach((item) => {
      if (!String(item.classList).includes("inactive"))
        item.setAttribute("draggable", "null");
      item.setAttribute("ondragstart", "null");
      if (!item.classList.contains("inactive")) {
        item.classList += " inactive";
      }
      item.style.cursor = "auto";
    });
    document.querySelectorAll(".object").forEach((item) => {
      if (item.classList.contains("hover")) {
        item.classList.remove("hover");
      }

      item.addEventListener("dblclick", deleteSpecifiedBlock);
      item.removeEventListener("mousedown", Hold);
      document.removeEventListener("mouseup", () =>
        cancelAnimationFrame(animationFrameId)
      );
      item.classList.add("red");

    });
  } else {
    document.querySelector(".mapcontrols").style.visibility = "visible";
    isEnabled = false;
    e.classList = "button";
    document.querySelectorAll('[class="button"]').forEach((item) => {
      item.style.pointerEvents = "auto";
      item.style.color = "white";
    });

    document.querySelectorAll(".object").forEach((item) => {
      item.classList.remove("red");
      item.style.borderStyle = "none";
      item.style.cursor = "e-resize";
      item.removeEventListener("dblclick", deleteSpecifiedBlock);

      item.addEventListener("mousedown", Hold);
      if (item.firstChild.firstChild) {
        item.classList += " hover";
      }
      document.addEventListener("mouseup", () => {
        cancelAnimationFrame(animationFrameId);
      });
    });

    const placedDevices = Array.from(document.querySelectorAll(".recieverID"));
    document.querySelectorAll(".devicecontainer").forEach((item) => {
      if (
        !placedDevices.find(
          (x) => x.textContent === item.children[0].textContent
        )
      ) {
        item.classList.remove("inactive");

        if (item.querySelector("p.offline")) {
          item.classList += " offline";
        }

        item.setAttribute("draggable", "true");
        item.setAttribute("ondragstart", "drag(event)");
        item.style.cursor = "grab";
      } else if (!String(item.classList).includes("inactive")) {
        item.classList += " inactive";
      }
    });
  }
}

function deleteSpecifiedBlock(e) {
  e.target.remove();
}

function createElementFromHTML(htmlString) {
  var div = document.createElement("div");
  div.innerHTML = htmlString.trim();

  // Change this to div.childNodes to support multiple top-level nodes.
  return div.firstChild;
}

function GenerateMapJsonString() {
  const obj = [];

  document
    .querySelectorAll("div.map>.object")
    .forEach((item) => obj.push(item.outerHTML));

  return JSON.stringify(obj);
}

function Save() {
  var json_string = GenerateMapJsonString();

  var link = document.createElement("a");
  link.download = "mapLayout.json";
  var blob = new Blob([json_string], { type: "text/plain" });
  link.href = window.URL.createObjectURL(blob);
  link.click();
}

function Load() {
  var element = document.createElement("div");
  element.innerHTML = '<input type="file">';
  var fileInput = element.firstChild;

  fileInput.addEventListener("change", function () {
    var file = fileInput.files[0];

    if (file.name.match(/\.(txt|json)$/)) {
      var reader = new FileReader();
      reader.onload = function () {
        const obj = JSON.parse(reader.result);

        fetch(serverIp + '/map', {
          method: 'POST',
          headers: {
            'Accept': 'application/json',
            'Content-Type': 'application/json',
            'Content-Length': reader.result.length
          },
          body: reader.result
        });

        document.querySelector(".map").innerHTML =
          '<canvas id="position-canvas" class="position-display"></canvas><div class="mapcontrols"><div class="mapbtn" onclick="zoom()">+</div> <div class="mapbtn move"> <svg xmlns="http://www.w3.org/2000/svg" height="1em" viewBox="0 0 512 512" > <!--! Font Awesome Free 6.4.2 by @fontawesome - https://fontawesome.com License - https://fontawesome.com/license (Commercial License) Copyright 2023 Fonticons, Inc. --> <style> svg { fill: #ffffff; } </style> <path d="M278.6 9.4c-12.5-12.5-32.8-12.5-45.3 0l-64 64c-9.2 9.2-11.9 22.9-6.9 34.9s16.6 19.8 29.6 19.8h32v96H128V192c0-12.9-7.8-24.6-19.8-29.6s-25.7-2.2-34.9 6.9l-64 64c-12.5 12.5-12.5 32.8 0 45.3l64 64c9.2 9.2 22.9 11.9 34.9 6.9s19.8-16.6 19.8-29.6V288h96v96H192c-12.9 0-24.6 7.8-29.6 19.8s-2.2 25.7 6.9 34.9l64 64c12.5 12.5 32.8 12.5 45.3 0l64-64c9.2-9.2 11.9-22.9 6.9-34.9s-16.6-19.8-29.6-19.8H288V288h96v32c0 12.9 7.8 24.6 19.8 29.6s25.7 2.2 34.9-6.9l64-64c12.5-12.5 12.5-32.8 0-45.3l-64-64c-9.2-9.2-22.9-11.9-34.9-6.9s-19.8 16.6-19.8 29.6v32H288V128h32c12.9 0 24.6-7.8 29.6-19.8s2.2-25.7-6.9-34.9l-64-64z" /> </svg> </div> <div class="mapbtn" onclick="zoomOut()">-</div> </div>';
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
      };

      reader.readAsText(file);
    } else {
      alert("File not supported, .txt or .json files only");
    }
  });

  fileInput.click();
}

function zoom() {
  zoomLevel *= 1.5;
  document.querySelectorAll(".object").forEach((item) => {

    item.style.height = item.clientHeight * 1.5 + "px";
    item.style.width = item.clientWidth * 1.5 + "px";


    var oldTop = Number(window.getComputedStyle(item).top.split("p")[0]);
    var oldLeft = Number(window.getComputedStyle(item).left.split("p")[0]);

    var newTop = oldTop * 1.5;
    var newLeft = oldLeft * 1.5;

    item.style.top = newTop + "px";
    item.style.left = newLeft + "px";
  });
}

function zoomOut() {
  zoomLevel /= 1.5;
  document.querySelectorAll(".object").forEach((item) => {

    item.style.height = item.clientHeight / 1.5 + "px";
    item.style.width = item.clientWidth / 1.5 + "px";


    var oldTop = Number(window.getComputedStyle(item).top.split("p")[0]);
    var oldLeft = Number(window.getComputedStyle(item).left.split("p")[0]);

    var newTop = oldTop / 1.5;
    var newLeft = oldLeft / 1.5;

    item.style.top = newTop + "px";
    item.style.left = newLeft + "px";
  });
}

function moveMap() {
  document.querySelector(".move").addEventListener("mousedown", Hold);
}

function AddOutline(selected) {
  if (selected.classList.contains("reciever")) {
    document.querySelectorAll("div.object:not(.reciever)").forEach(item => {
      item.removeEventListener("mousedown", Hold);
    });
    selected.classList += " reciverhover";
    selected.querySelector("p").style.visibility = "visible";
    document.querySelector(`div.devicecontainer[id="${selected.querySelector("p").textContent}"]`).classList += " reciverhover";
    return;
  }

  selected.classList += " reciverhover";
  var obj = Array.from(document.querySelectorAll(".recieverID")).find((e) => e.textContent == selected.id);
  if (obj) {
    obj.style.visibility = "visible";
    obj.parentElement.classList += " reciverhover";
  }
}

function RemoveOutline(selected) {
  if (selected.classList.contains("reciever")) {
    document.querySelectorAll("div.object:not(.reciever)").forEach(item => {
      item.addEventListener("mousedown", Hold);
    });
    selected.classList.remove("reciverhover");
    selected.querySelector("p").style.visibility = "hidden";

    document.querySelector(`div.devicecontainer[id="${selected.querySelector("p").textContent}"]`).classList.remove("reciverhover");

    return;
  }
  selected.classList.remove("reciverhover");
  var obj = Array.from(document.querySelectorAll(".recieverID")).find((e) => e.textContent == selected.id);
  if (obj) {
    obj.style.visibility = "hidden";
    obj.parentElement.classList.remove("reciverhover");
  }
}

function OpenPopupMenu(e) {

  var menu = document.querySelector(".popup-menu");
  if (menu.style.display === "flex") {
    menu.style.display = "none";
    e.style.color = "white";
  }
  else {
    menu.style.display = "flex";
    e.style.color = "#ffe205";
  } notificationcontainer
}

function AddNewBlock() {
  const div = document.createElement("div");
  div.classList = "object hover";
  div.innerHTML = `<div class="measuring"style="visibility:hidden;"><p><</p><div></div><p class="measurement"></p><div></div><p>></p></div><div class="height-measuring" style="visibility: hidden;"><p>></p><div></div><p class="measurement-height"></p><div></div><p><</p></div>`;
  div.addEventListener("mousedown", Hold);
  document.querySelector(".map").insertBefore(div, document.querySelector(".position-display"));
  document.addEventListener("mouseup", () => {
    cancelAnimationFrame(animationFrameId);
  });
}

function AddNewRoom() {
  const div = document.createElement("div");
  div.classList = "object hover room";
  div.innerHTML = `<div class="measuring"style="visibility:visible;"><input type="text" name="roomName" placeholder="Enter room name"><p><</p><div></div><p class="measurement">${pxToM(500)}m</p><div></div><p>></p></div><div class="height-measuring" style="visibility: visible;"><p>></p><div></div><p class="measurement-height">${pxToM(200)}m</p><div></div><p><</p></div>`;
  div.addEventListener("mousedown", Hold);
  document.querySelector(".map").insertBefore(div, document.querySelector(".position-display"));
  document.addEventListener("mouseup", () => {
    cancelAnimationFrame(animationFrameId);
  });
}
