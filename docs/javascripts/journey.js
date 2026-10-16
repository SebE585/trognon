/* Guided journey: turn the notebook block of a scene into a side panel (a full-screen drawer on a phone).
   Progressive enhancement: without this script the notebook stays an ordinary collapsible block under the scene. */
(function () {
  "use strict";
  function init() {
    // Heading permalinks are a mouse convenience: on a scene they would be five tab stops before the first choice.
    if (document.querySelector(".journey-trail")) {
      Array.prototype.forEach.call(document.querySelectorAll("a.headerlink"), function (a) { a.setAttribute("tabindex", "-1"); });
    }
    var nb = document.querySelector("details.journey-notebook");
    if (!nb) return;
    var label = nb.getAttribute("data-label") || "Notebook";
    var back = nb.getAttribute("data-back") || "Back";

    var panel = document.createElement("aside");
    panel.className = "journey-panel";
    panel.setAttribute("aria-label", label);
    panel.setAttribute("aria-hidden", "true");

    var head = document.createElement("div");
    head.className = "journey-panel__head";
    var title = document.createElement("strong");
    title.textContent = label;
    var close = document.createElement("button");
    close.type = "button";
    close.className = "journey-panel__close";
    close.textContent = back;
    head.appendChild(title);
    head.appendChild(close);

    var body = document.createElement("div");
    body.className = "journey-panel__body md-typeset";
    Array.prototype.forEach.call(nb.children, function (child) {
      if (child.tagName !== "SUMMARY") body.appendChild(child.cloneNode(true));
    });
    panel.appendChild(head);
    panel.appendChild(body);

    var toggle = document.createElement("button");
    toggle.type = "button";
    toggle.className = "journey-panel-toggle";
    toggle.textContent = label;
    toggle.setAttribute("aria-expanded", "false");

    function setOpen(open) {
      panel.classList.toggle("open", open);
      panel.setAttribute("aria-hidden", open ? "false" : "true");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
      if (open) close.focus(); else toggle.focus();
    }
    toggle.addEventListener("click", function () { setOpen(!panel.classList.contains("open")); });
    close.addEventListener("click", function () { setOpen(false); });
    document.addEventListener("keydown", function (e) {
      if (e.key === "Escape" && panel.classList.contains("open")) setOpen(false);
    });

    document.body.appendChild(panel);
    // The tab is fixed to the edge of the screen, so where it sits in the document only decides when the keyboard reaches it:
    // right after the title, not after everything else.
    var h1 = document.querySelector("article h1");
    if (h1) h1.insertAdjacentElement("afterend", toggle); else document.body.appendChild(toggle);
    document.documentElement.classList.add("journey-js");
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init); else init();
})();
