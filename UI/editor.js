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
    } else {
      item.classList += " inactive";
    }
  });

  document.querySelectorAll(".object").forEach((item) => {
    item.addEventListener("mousedown", Hold);
    document.addEventListener("mouseup", () =>
      cancelAnimationFrame(animationFrameId)
    );
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
  const moveObject = () => {
    let newOffsetY = Math.round(
      Number(window.getComputedStyle(obj).top.split("p")[0]) + offsetY
    );
    let newOffsetX = Math.round(
      Number(window.getComputedStyle(obj).left.split("p")[0]) + offsetX
    );

    obj.style.top = newOffsetY + "px";
    obj.style.left = newOffsetX + "px";
    animationFrameId = requestAnimationFrame(moveObject);
  };

  moveObject();
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
