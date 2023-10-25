let timeoutId;

let oldCursorPosX = 0,
  oldCursorPosY = 0,
  offsetX,
  offsetY;

function detectMouseMovement(event) {
  offsetX = event.clientX - oldCursorPosX;
  offsetY = event.clientY - oldCursorPosY;
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

function pxToM(px, modifier = 0.015) {
  return (px * modifier).toFixed(1);
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
  //document.querySelector(".sidemenu").innerHTML +=
  //' <a class="button" onclick="SnapToGrid()" href="#">Snap to Grid</a><a class="button" onclick="AddNewBlock()" href="#">Add New Block</a><a class="button" onclick="Deleteblock()" href="#">Delete Block</a><a class="button" onclick="Save()" href="#">Save</a><a class="button" onclick="Load()" href="#">Load</a>';

  btn.removeEventListener("click", EditMap);
  btn.addEventListener("click", ExitEditor);

  const placedDevices = Array.from(document.querySelectorAll(".recieverID"));
  document.querySelectorAll(".devicecontainer").forEach((item) => {
    if (
      !placedDevices.find((x) => x.textContent === item.children[0].textContent)
    ) {
      item.setAttribute("draggable", "true");
      item.setAttribute("ondragstart", "drag(event)");
      item.style.cursor = "drag";
    } else {
      item.classList += " inactive";
    }
  });

  document.querySelectorAll(".object").forEach((item) => {
    item.addEventListener("mousedown", Hold);
    if (!item.hasChildNodes()) {
      item.classList += " hover";
      item.innerHTML = `<div class="measuring"style="visibility:${
        item.offsetWidth > 60 ? "visible" : "hidden"
      };"><p><</p><div></div><p class="measurement"> ${pxToM(
        item.offsetWidth
      )}m </p><div></div><p>></p></div>`;

      item.innerHTML += `<div class="height-measuring" style="visibility:${
        item.offsetHeight > 60 ? "visible" : "hidden"
      };">
      <p>></p>
      <div></div>
      <p class="measurement-height">${pxToM(item.offsetHeight)}m</p>
      <div></div>
      <p><</p>
    </div>`;
    }

    document.addEventListener("mouseup", () => {
      cancelAnimationFrame(animationFrameId);
    });
  });
}

function ExitEditor() {
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

  document.querySelectorAll(".object").forEach((item) => {
    item.removeEventListener("mousedown", Hold);
    console.log(item.classList);
    if (item.classList[1] !== "reciever") {
      item.classList = "object";
      item.innerHTML = "";
    }
    document.removeEventListener("mouseup", () =>
      cancelAnimationFrame(animationFrameId)
    );
  });

  document.querySelector(".button").removeEventListener("click", ExitEditor);
  document.querySelector(".button").addEventListener("click", EditMap);
}

let animationFrameId;

function Hold(e) {
  const obj = e.target;
  if (e.layerX > obj.offsetWidth - 5) {
    resizeObjectRight();
  } else if (e.layerX < 5) {
    resizeObjectLeft();
  } else if (e.layerY > obj.offsetHeight - 5) {
    resizeObjectBottom();
  } else {
    moveObject();
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
  deviceDiv.classList += " inactive";
  deviceDiv.removeAttribute("draggable");
  deviceDiv.removeAttribute("ondragstart");

  const newReciver = document.createElement("div");
  newReciver.classList += "object reciever";
  const p = document.createElement("p");
  p.classList += "recieverID";
  newReciver.appendChild(p);
  newReciver.children[0].textContent = transferdData;
  newReciver.style.top = ev.layerY + "px";
  newReciver.style.left = ev.layerX + "px";
  newReciver.addEventListener("mousedown", Hold);

  ev = ev.target;
  if (ev.classList[0] !== "map") {
    ev = ev.parentElement;
  }

  ev.appendChild(newReciver);
}

function AddNewBlock() {
  const div = document.createElement("div");
  div.classList = "object hover";
  div.innerHTML = `<div class="measuring"style="visibility:hidden;"><p><</p><div></div><p class="measurement"></p><div></div><p>></p></div><div class="height-measuring" style="visibility: hidden;"><p>></p><div></div><p class="measurement-height"></p><div></div><p><</p></div>`;
  div.addEventListener("mousedown", Hold);
  document.querySelector(".map").appendChild(div);
  document.addEventListener("mouseup", () => {
    cancelAnimationFrame(animationFrameId);
  });
  SnapToGrid();
}

let isEnabled = false;
function deleteBlocks(e) {
  if (!isEnabled) {
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
      item.classList += " inactive";
      item.style.cursor = "auto";
    });
    document.querySelectorAll(".object").forEach((item) => {
      let classes = String(item.classList);
      if (classes.includes("hover")) {
        classes = classes.replace(" hover", "");
        item.classList = classes;
      }

      item.addEventListener("dblclick", deleteSpecifiedBlock);
      item.removeEventListener("mousedown", Hold);
      document.removeEventListener("mouseup", () =>
        cancelAnimationFrame(animationFrameId)
      );

      item.style.backgroundColor = "red";
      item.style.border = "1px solid white";
      item.style.cursor = "crosshair";
    });
  } else {
    isEnabled = false;
    e.classList = "button";
    document.querySelectorAll('[class="button"]').forEach((item) => {
      item.style.pointerEvents = "auto";
      item.style.color = "white";
    });

    document.querySelectorAll(".object").forEach((item) => {
      String(item.classList) === "object reciever"
        ? (item.style.backgroundColor = "#ffe205")
        : (item.style.backgroundColor = "white");
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
        item.classList = "devicecontainer";
        console.log("here");
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

function Save() {
  const obj = [];

  document
    .querySelectorAll(".object")
    .forEach((item) => obj.push(item.outerHTML));

  var json_string = JSON.stringify(obj, undefined, 2);

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
        console.log(JSON.parse(reader.result));
        const obj = JSON.parse(reader.result);
        document.querySelector(".map").innerHTML = "";
        obj.forEach((obj) => {
          const div = createElementFromHTML(obj);
          div.addEventListener("mousedown", Hold);
          document.querySelector(".map").appendChild(div);
        });
      };

      reader.readAsText(file);
    } else {
      alert("File not supported, .txt or .json files only");
    }
  });

  fileInput.click();
}
