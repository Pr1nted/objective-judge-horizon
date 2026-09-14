import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.backends.headless.HeadlessFiles;
import com.unciv.UncivGame;
import com.unciv.logic.GameInfo;
import com.unciv.logic.GameStarter;
import com.unciv.logic.civilization.Civilization;
import com.unciv.logic.civilization.PlayerType;
import com.unciv.logic.map.MapParameters;
import com.unciv.logic.map.MapSize;
import com.unciv.models.metadata.BaseRuleset;
import com.unciv.models.metadata.GameParameters;
import com.unciv.models.metadata.GameSettings;
import com.unciv.models.metadata.GameSetupInfo;
import com.unciv.models.metadata.Player;
import com.unciv.models.ruleset.Ruleset;
import com.unciv.models.ruleset.RulesetCache;
import com.unciv.models.ruleset.nation.Nation;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Locale;

/**
 * OJH driver for Unciv: turns per minute, every civilization AI, no window.
 *
 * Starts a new game through Unciv's own GameStarter with real major nations from the
 * base ruleset, all played by Unciv's AI plus one spectator, and advances it with
 * GameInfo.nextTurn, the same call the game makes when a turn ends. One JSON line per
 * turn and one summary line on stdout, numbers always with a decimal point (Locale.ROOT).
 *
 * Unciv reads its rulesets from jsons/ relative to the working directory: run it from a
 * folder holding the jar's jsons/ (extract it with `jar xf Unciv.jar jsons`).
 *
 *   javac -cp Unciv.jar -d out drivers/unciv/UncivTpm.java
 *   java -Djava.awt.headless=true -cp out:Unciv.jar UncivTpm <civs> <turns> <tiny|small|medium|large|huge>
 */
public final class UncivTpm {
    public static void main(String[] args) {
        int civs = args.length > 0 ? Integer.parseInt(args[0]) : 8;
        int turns = args.length > 1 ? Integer.parseInt(args[1]) : 100;
        String size = args.length > 2 ? args[2] : "small";

        long bootStart = System.nanoTime();
        UncivGame game = new UncivGame(true);
        UncivGame.Companion.setCurrent(game);
        GameSettings settings = new GameSettings();
        settings.setShowTutorials(false);
        settings.setTurnsBetweenAutosaves(1_000_000);
        game.setSettings(settings);
        // Unciv reads its rulesets through libGDX's file system, which a desktop launch sets up
        // with its window. Headless, nothing does, and every ruleset loads empty.
        Gdx.files = new HeadlessFiles();
        RulesetCache.INSTANCE.loadRulesets(true, true);
        for (String name : RulesetCache.INSTANCE.keySet()) {
            Ruleset loaded = RulesetCache.INSTANCE.get(name);
            System.err.printf(Locale.ROOT, "ruleset %s: %d speeds, %d nations%n", name,
                    loaded.getSpeeds().size(), loaded.getNations().size());
        }

        Ruleset ruleset = RulesetCache.INSTANCE.get(BaseRuleset.Civ_V_GnK.getFullName());
        if (ruleset == null) ruleset = RulesetCache.INSTANCE.getVanillaRuleset();

        ArrayList<Player> players = new ArrayList<>();
        for (Nation nation : ruleset.getNations().values()) {
            if (players.size() >= civs) break;
            if (!nation.isMajorCiv()) continue;
            players.add(new Player(nation.getName(), PlayerType.AI, ""));
        }
        players.add(new Player("Spectator", PlayerType.Human, ""));

        GameParameters parameters = new GameParameters();
        parameters.setBaseRuleset(ruleset.getName());
        parameters.setDifficulty("King");
        parameters.setSpeed("Quick");
        parameters.setNoBarbarians(true);
        parameters.setNumberOfCityStates(0);
        parameters.setShufflePlayerOrder(false);
        parameters.setPlayers(players);

        MapParameters map = new MapParameters();
        switch (size) {
            case "tiny": map.setMapSize(MapSize.Companion.getTiny()); break;
            case "medium": map.setMapSize(MapSize.Companion.getMedium()); break;
            case "large": map.setMapSize(MapSize.Companion.getLarge()); break;
            case "huge": map.setMapSize(MapSize.Companion.getHuge()); break;
            default: map.setMapSize(MapSize.Companion.getSmall()); break;
        }
        map.setNoRuins(true);

        GameInfo info = GameStarter.Companion.startNewGame(new GameSetupInfo(parameters, map));
        game.setGameInfo(info);
        double bootSeconds = (System.nanoTime() - bootStart) / 1e9;

        // GameInfo.nextTurn(progress, isOnline) has Kotlin default arguments; javac cannot see the
        // synthetic nextTurn$default that applies them, but reflection can. Mask 3 = both defaults.
        Method nextTurn;
        try {
            Class<?> progress = Class.forName("com.unciv.ui.screens.worldscreen.status.NextTurnProgress");
            nextTurn = GameInfo.class.getMethod("nextTurn$default", GameInfo.class, progress, boolean.class, int.class, Object.class);
        } catch (ReflectiveOperationException e) {
            throw new IllegalStateException("this Unciv has no GameInfo.nextTurn$default", e);
        }
        double total = 0;
        int played = 0;
        for (int i = 0; i < turns; i++) {
            long t0 = System.nanoTime();
            try {
                nextTurn.invoke(null, info, null, false, 3, null);
            } catch (ReflectiveOperationException e) {
                throw new IllegalStateException("nextTurn failed on turn " + (i + 1), e);
            }
            double seconds = (System.nanoTime() - t0) / 1e9;
            total += seconds;
            played++;
            int alive = 0, cities = 0;
            for (Civilization civ : info.getCivilizations()) {
                if (civ.isMajorCiv() && !civ.isDefeated()) {
                    alive++;
                    cities += civ.getCities().size();
                }
            }
            System.out.printf(Locale.ROOT, "{\"turn\": %d, \"seconds\": %.6f, \"game_turn\": %d, \"major_civs_alive\": %d, \"cities\": %d}%n",
                    i + 1, seconds, info.getTurns(), alive, cities);
        }
        int tiles = info.getTileMap().getValues().size();
        System.out.printf(Locale.ROOT, "{\"summary\": {\"game\": \"Unciv\", \"ruleset\": \"%s\", \"civs\": %d, \"map_size\": \"%s\", \"tiles\": %d, "
                        + "\"turns\": %d, \"boot_seconds\": %.3f, \"turn_seconds\": %.3f, \"tpm\": %.2f, \"java\": \"%s\"}}%n",
                ruleset.getName(), players.size() - 1, size, tiles, played, bootSeconds, total,
                total > 0 ? played / (total / 60.0) : 0.0, System.getProperty("java.version"));
        System.exit(0);
    }
}
