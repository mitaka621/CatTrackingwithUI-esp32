window.addEventListener("load", () =>
  document.querySelectorAll(".button2").forEach(b => b.addEventListener("click", unwrap))
);

let isUnwrapped = true;
function unwrap() {
  const btns = document.querySelectorAll(".button2");
  if (isUnwrapped) {
    document.querySelector(".container").style.height = "0";
    document.querySelector(".container2").style.height = "100%";
    btns[1].removeEventListener("click", unwrap);
    btns[0].addEventListener("click", unwrap);
    isUnwrapped = false;
  } else {
    btns[0].removeEventListener("click", unwrap);
    btns[1].addEventListener("click", unwrap);
    document.querySelector(".container").style.height = "100%";
    document.querySelector(".container2").style.height = "0";
    isUnwrapped = true;
  }
}

function deleteNotification(e) {
  e.parentElement.style.maxHeight = 0;
  e.parentElement.style.maxWidth = 0;
  const grandparent = e.parentElement.parentElement;
  setTimeout(() => {
    e.parentElement.remove();
    if (grandparent.innerText === "") {
      grandparent.innerHTML =
        '<h2 id="no-notifications-alert" style="color:white; margin-top:0.5em">No unseen notifications.</h2>';
    }
  }, 500);
}
