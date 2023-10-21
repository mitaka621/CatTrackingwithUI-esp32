let isUnwrapped = true;
function unwrap() {
  if (isUnwrapped) {
    console.log(document.querySelector(".container"));
    document.querySelector(".container").style.height = "0";
    document.querySelector(".container2").style.height = "100%";

    isUnwrapped = false;
  } else {
    document.querySelector(".container").style.height = "100%";
    document.querySelector(".container2").style.height = "0";
    isUnwrapped = true;
  }
}

function deleteNotification(e) {
  e.parentElement.style.maxHeight = 0;
  e.parentElement.style.maxWidth = 0;
  //e.parentElement.remove();
}
