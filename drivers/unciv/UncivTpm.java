import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.backends.headless.HeadlessFiles;
import com.unciv.UncivGame;
import com.unciv.logic.GameInfo;
import com.unciv.logic.GameStarter;
import com.unciv.logic.files.UncivFiles;
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
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Locale;

public final class UncivTpm {
    public static void main(String[] args) {
        int civs = args.length > 0 ? Integer.parseInt(args[0]) : 8;
        int turns = args.length > 1 ? Integer.parseInt(args[1]) : 100;
        String size = args.length > 2 ? args[2] : "small";
        boolean footprint = args.length > 3 && args[3].equals("footprint");
        boolean net = args.length > 3 && args[3].equals("net");

        long bootStart = System.nanoTime();
        UncivGame game = new UncivGame(true);
        UncivGame.Companion.setCurrent(game);
        GameSettings settings = new GameSettings();
        settings.setShowTutorials(false);
        settings.setTurnsBetweenAutosaves(1_000_000);
        game.setSettings(settings);
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
        int cityStates = Math.max(0, civs - players.size());
        players.add(new Player("Spectator", PlayerType.Human, ""));

        GameParameters parameters = new GameParameters();
        parameters.setBaseRuleset(ruleset.getName());
        parameters.setDifficulty("King");
        parameters.setSpeed("Quick");
        parameters.setNoBarbarians(true);
        parameters.setNumberOfCityStates(cityStates);
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
            if (net) {
                int upload = UncivFiles.Companion.gameInfoToString(info, Boolean.TRUE, false).getBytes(StandardCharsets.UTF_8).length;
                System.out.printf(Locale.ROOT, "OJH data %d %d%n", upload, upload);
                System.out.printf(Locale.ROOT, "OJH turn %d%n", i + 1);
            }
        }
        if (footprint) {
            long saveStart = System.nanoTime();
            String saved = UncivFiles.Companion.gameInfoToString(info, Boolean.TRUE, false);
            double saveSeconds = (System.nanoTime() - saveStart) / 1e9;
            long loadStart = System.nanoTime();
            UncivFiles.Companion.gameInfoFromString(saved);
            double loadSeconds = (System.nanoTime() - loadStart) / 1e9;
            System.out.printf(Locale.ROOT, "OJH save %d %.6f%n", saved.getBytes(StandardCharsets.UTF_8).length, saveSeconds);
            System.out.printf(Locale.ROOT, "OJH load %.6f%n", loadSeconds);
        }
        int tiles = info.getTileMap().getValues().size();
        int aiCivs = 0;
        for (Civilization civ : info.getCivilizations()) {
            if (!civ.isBarbarian() && !civ.isSpectator()) aiCivs++;
        }
        System.out.printf(Locale.ROOT, "{\"summary\": {\"game\": \"Unciv\", \"ruleset\": \"%s\", \"civs\": %d, \"map_size\": \"%s\", \"tiles\": %d, "
                        + "\"turns\": %d, \"boot_seconds\": %.3f, \"turn_seconds\": %.3f, \"tpm\": %.2f, \"java\": \"%s\"}}%n",
                ruleset.getName(), aiCivs, size, tiles, played, bootSeconds, total,
                total > 0 ? played / (total / 60.0) : 0.0, System.getProperty("java.version"));
        System.exit(0);
    }
}
