const modes = {
  solo: {
    title: "Ten-round memory gauntlet",
    text: "Beat increasingly dense hazard layouts, preserve health, and build a score across ten fresh mazes. Disable buffs for a pure memory challenge.",
    points: ["WASD movement", "Optional power-up progression", "Speed and health scoring"]
  },
  versus: {
    title: "One keyboard. One vault. One winner.",
    text: "Choose a 3, 5, 7, or 10-round match. Player One uses WASD and Player Two uses the arrow keys. The first arrival scores highest; the rival gets a five-second final chance.",
    points: ["Configurable match length", "Competitive personal buff drafts", "Final accumulated-score leaderboard"]
  }
};

const panel = document.querySelector("#mode-copy");
document.querySelectorAll(".mode-tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".mode-tab").forEach((item) => {
      item.classList.toggle("active", item === tab);
      item.setAttribute("aria-selected", item === tab ? "true" : "false");
    });
    const mode = modes[tab.dataset.mode];
    panel.innerHTML = `<h3>${mode.title}</h3><p>${mode.text}</p><ul>${mode.points.map(point => `<li>${point}</li>`).join("")}</ul>`;
  });
});
