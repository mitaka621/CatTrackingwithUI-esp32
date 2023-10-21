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
  const map = document.querySelector(".map");
  map.setAttribute("class", "map edit");
  document.querySelector(".sidemenu").innerHTML +=
    ' <a class="button" onclick="SnapToGrid()" href="#">Snap to Grid</a><a class="button" onclick="AddNewBlock()" href="#">Add New Block</a><a class="button" onclick="Deleteblock()" href="#">Delete Block</a><a class="button" onclick="Save()" href="#">Save</a><a class="button" onclick="Load()" href="#">Load</a>';

  const btn = document.querySelector(".button");
  btn.removeEventListener("click", EditMap);
  btn.addEventListener("click", ExitEditor);

  document.querySelectorAll(".object").forEach((item) => {
    item.addEventListener("mousedown", Hold);
    document.addEventListener("mouseup", () =>
      cancelAnimationFrame(animationFrameId)
    );
  });
}

function ExitEditor() {
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
    console.log(posY);
    console.log(posX);
    if (posY % 10 != 0) {
      if (posY % 10 < 5) {
        posY -= posY % 10;
      } else {
        posY += 10 - (posY % 10);
      }
    }

    if (posX % 10 != 0) {
      if (posX % 10 < 5) {
        posX -= posX % 10;
      } else {
        posX += 10 - (posX % 10);
      }
    }

    console.log(posY);
    console.log(posX);

    block.style.top = posY + "px";
    block.style.left = posX + "px";
  });
}
