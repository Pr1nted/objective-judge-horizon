import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.backends.lwjgl3.Lwjgl3Application;
import com.badlogic.gdx.backends.lwjgl3.Lwjgl3ApplicationConfiguration;
import com.badlogic.gdx.backends.lwjgl3.Lwjgl3Graphics;
import com.unciv.UncivGame;
import com.unciv.app.desktop.DesktopDisplay;
import com.unciv.app.desktop.DesktopFont;
import com.unciv.app.desktop.DesktopLogBackend;
import com.unciv.app.desktop.DesktopSaverLoader;
import com.unciv.logic.GameInfo;
import com.unciv.logic.GameStarter;
import com.unciv.logic.civilization.Civilization;
import com.unciv.logic.civilization.PlayerType;
import com.unciv.logic.files.UncivFiles;
import com.unciv.logic.map.MapParameters;
import com.unciv.logic.map.MapSize;
import com.unciv.models.metadata.BaseRuleset;
import com.unciv.models.metadata.GameParameters;
import com.unciv.models.metadata.GameSetupInfo;
import com.unciv.models.metadata.Player;
import com.unciv.models.ruleset.Ruleset;
import com.unciv.models.ruleset.RulesetCache;
import com.unciv.models.ruleset.nation.Nation;
import com.unciv.ui.components.fonts.Fonts;
import com.unciv.ui.screens.basescreen.BaseScreen;
import com.unciv.ui.screens.mainmenuscreen.MainMenuScreen;
import com.unciv.ui.screens.worldscreen.WorldScreen;
import com.unciv.ui.screens.worldscreen.worldmap.WorldMapHolder;
import com.unciv.utils.Display;
import com.unciv.utils.Log;
import kotlin.coroutines.Continuation;
import kotlin.coroutines.EmptyCoroutineContext;
import kotlinx.coroutines.BuildersKt;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Locale;
import java.util.Set;

public final class UncivFps {
    static final String[] SCENES = {"menu", "map-start", "map-out", "map-in", "map-pan", "panel", "end-turn", "map-late"};
    static final Set<String> reported = Collections.synchronizedSet(new LinkedHashSet<>());

    static synchronized void say(String text) {
        System.out.println(text);
        System.out.flush();
    }

    static void summary(String scene, ArrayList<Double> frames) {
        if (frames.size() < 2) {
            say("OJH noscene " + scene + " too few frames were drawn to time");
            reported.add(scene);
            return;
        }
        ArrayList<Double> sorted = new ArrayList<>(frames);
        Collections.sort(sorted);
        int n = sorted.size();
        double total = 0;
        for (double f : sorted) total += f;
        int slowCount = Math.max(1, n / 100);
        double slow = 0;
        for (int i = n - slowCount; i < n; i++) slow += sorted.get(i);
        say(String.format(Locale.ROOT, "OJH scene %s %d %.5f %.3f %.3f %.3f %.2f", scene, n, total,
                sorted.get(Math.min(n - 1, (int) (0.50 * (n - 1)))) * 1000.0,
                sorted.get(Math.min(n - 1, (int) (0.95 * (n - 1)))) * 1000.0,
                sorted.get(Math.min(n - 1, (int) (0.99 * (n - 1)))) * 1000.0,
                slow > 0 ? slowCount / slow : 0.0));
        reported.add(scene);
    }

    static GameInfo newGame(int civs, String size) {
        Ruleset ruleset = RulesetCache.INSTANCE.get(BaseRuleset.Civ_V_GnK.getFullName());
        if (ruleset == null) ruleset = RulesetCache.INSTANCE.getVanillaRuleset();
        ArrayList<Player> players = new ArrayList<>();
        boolean human = false;
        for (Nation nation : ruleset.getNations().values()) {
            if (players.size() >= civs) break;
            if (!nation.isMajorCiv()) continue;
            players.add(new Player(nation.getName(), human ? PlayerType.AI : PlayerType.Human, ""));
            human = true;
        }
        int cityStates = Math.max(0, civs - players.size());
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
        return GameStarter.Companion.startNewGame(new GameSetupInfo(parameters, map));
    }

    static final class Game extends UncivGame {
        final double seconds;
        final int turns;
        final int civs;
        final String size;
        final ArrayList<Double> frames = new ArrayList<>();
        String scene;
        int warm;
        long last;
        long sceneEnd;
        long warmUntil;
        int stage;
        volatile WorldScreen loaded;
        volatile GameInfo lateGame;
        volatile boolean turnsDone;
        volatile String failure;
        GameInfo started;

        Game(double seconds, int turns, int civs, String size) {
            super(false);
            this.seconds = seconds;
            this.turns = turns;
            this.civs = civs;
            this.size = size;
        }

        @Override
        public void render() {
            long now = System.nanoTime();
            if (scene != null && last != 0) {
                if (warm > 0) warm--;
                else frames.add((now - last) / 1e9);
            }
            last = now;
            super.render();
            try {
                step(System.nanoTime());
            } catch (Throwable e) {
                failure = e.toString();
            }
            if (failure != null) finish(failure);
        }

        void begin(String name, int warmup) {
            scene = name;
            frames.clear();
            warm = warmup;
            sceneEnd = 0;
            warmUntil = System.nanoTime() + 2_000_000_000L;
        }

        boolean done(long now) {
            if (warm > 0 || now < warmUntil) {
                frames.clear();
                return false;
            }
            if (sceneEnd == 0) sceneEnd = now + (long) (seconds * 1e9);
            return now >= sceneEnd;
        }

        void report() {
            summary(scene, frames);
            scene = null;
        }

        WorldMapHolder holder() {
            return ((WorldScreen) getScreen()).getMapHolder();
        }

        void load(GameInfo info) {
            loaded = null;
            Thread t = new Thread(() -> {
                try {
                    Method m = UncivGame.class.getMethod("loadGame$default", UncivGame.class, GameInfo.class,
                            Class.forName("com.unciv.ui.screens.worldscreen.unit.AutoPlay"), boolean.class,
                            Continuation.class, int.class, Object.class);
                    Object screen = BuildersKt.runBlocking(EmptyCoroutineContext.INSTANCE,
                            (scope, continuation) -> {
                                try {
                                    return m.invoke(null, this, info, null, false, continuation, 6, null);
                                } catch (ReflectiveOperationException e) {
                                    throw new IllegalStateException(e);
                                }
                            });
                    loaded = (WorldScreen) screen;
                } catch (Throwable e) {
                    failure = "loading the game failed: " + e;
                }
            });
            t.setDaemon(true);
            t.start();
        }

        boolean openPanel(Civilization civ) {
            String[] candidates = {"com.unciv.ui.screens.pickerscreens.TechPickerScreen",
                    "com.unciv.ui.screens.victoryscreen.VictoryScreen"};
            String why = "no overview screen could be opened from outside the game";
            for (String name : candidates) {
                try {
                    Class<?> type = Class.forName(name);
                    for (Constructor<?> c : type.getConstructors()) {
                        Class<?>[] params = c.getParameterTypes();
                        Object[] args = new Object[params.length];
                        boolean usable = true;
                        for (int i = 0; i < params.length; i++) {
                            if (params[i] == Civilization.class) args[i] = civ;
                            else if (params[i] == WorldScreen.class) args[i] = getScreen();
                            else if (params[i] == boolean.class) args[i] = false;
                            else if (params[i] == int.class) args[i] = i == params.length - 2 ? ~1 : 0;
                            else if (params[i] == float.class) args[i] = 0f;
                            else if (!params[i].isPrimitive()) args[i] = null;
                            else usable = false;
                        }
                        if (!usable) continue;
                        try {
                            Object screen = c.newInstance(args);
                            setScreen((BaseScreen) screen);
                            return true;
                        } catch (Throwable e) {
                            why = name.substring(name.lastIndexOf('.') + 1) + ": " + e.getCause();
                        }
                    }
                } catch (Throwable e) {
                    why = e.toString();
                }
            }
            say("OJH noscene panel " + why);
            reported.add("panel");
            return false;
        }

        static Civilization humanCiv(GameInfo info) {
            for (Civilization civ : info.getCivilizations()) {
                if (civ.getPlayerType() == PlayerType.Human) return civ;
            }
            return info.getCivilizations().get(0);
        }

        void step(long now) {
            switch (stage) {
                case 0:
                    if (getScreen() instanceof MainMenuScreen) {
                        ((Lwjgl3Graphics) Gdx.graphics).getWindow().focusWindow();
                        say("OJH renderer " + Gdx.graphics.getGLVersion().getRendererString() + ", OpenGL "
                                + Gdx.graphics.getGLVersion().getMajorVersion() + "." + Gdx.graphics.getGLVersion().getMinorVersion()
                                + " via LWJGL3");
                        say("OJH resolution " + Gdx.graphics.getBackBufferWidth() + "x" + Gdx.graphics.getBackBufferHeight());
                        say("OJH vsync off");
                        begin("menu", 60);
                        stage = 1;
                    }
                    break;
                case 1:
                    if (done(now)) {
                        report();
                        started = newGame(civs, size);
                        load(started);
                        stage = 2;
                    }
                    break;
                case 2:
                    if (loaded != null && getScreen() == loaded) {
                        begin("map-start", 90);
                        stage = 3;
                    }
                    break;
                case 3:
                    if (done(now)) {
                        report();
                        holder().setScale(holder().getMinZoom());
                        begin("map-out", 60);
                        stage = 4;
                    }
                    break;
                case 4:
                    if (done(now)) {
                        report();
                        holder().setScale(holder().getMaxZoom());
                        begin("map-in", 60);
                        stage = 5;
                    }
                    break;
                case 5:
                    if (done(now)) {
                        report();
                        holder().setScale(1f);
                        begin("map-pan", 60);
                        stage = 6;
                    }
                    break;
                case 6: {
                    WorldMapHolder h = holder();
                    h.setScrollX(h.getScrollX() + 6f);
                    h.updateVisualScroll();
                    if (done(now)) {
                        report();
                        WorldScreen world = (WorldScreen) getScreen();
                        if (openPanel(humanCiv(started))) {
                            begin("panel", 60);
                            stage = 7;
                        } else {
                            stage = 8;
                        }
                        loaded = world;
                    }
                    break;
                }
                case 7:
                    if (done(now)) {
                        report();
                        setScreen(loaded);
                        stage = 8;
                    }
                    break;
                case 8: {
                    GameInfo clone = started.clone();
                    Thread t = new Thread(() -> {
                        try {
                            Class<?> progress = Class.forName("com.unciv.ui.screens.worldscreen.status.NextTurnProgress");
                            Method next = GameInfo.class.getMethod("nextTurn$default", GameInfo.class, progress, boolean.class,
                                    int.class, Object.class);
                            for (int i = 0; i < turns; i++) next.invoke(null, clone, null, false, 3, null);
                            lateGame = clone;
                            turnsDone = true;
                        } catch (Throwable e) {
                            failure = "playing turns failed: " + e;
                        }
                    });
                    t.setDaemon(true);
                    begin("end-turn", 10);
                    t.start();
                    stage = 9;
                    break;
                }
                case 9:
                    if (turnsDone) {
                        report();
                        load(lateGame);
                        stage = 10;
                    }
                    break;
                case 10:
                    if (loaded != null && getScreen() == loaded) {
                        begin("map-late", 90);
                        stage = 11;
                    }
                    break;
                case 11:
                    if (done(now)) {
                        report();
                        finish(null);
                    }
                    break;
                default:
                    break;
            }
        }

        void finish(String why) {
            for (String s : SCENES) {
                if (!reported.contains(s)) say("OJH noscene " + s + " " + (why != null ? why : "not reached"));
            }
            System.exit(why == null ? 0 : 1);
        }
    }

    public static void main(String[] args) {
        double seconds = args.length > 0 ? Double.parseDouble(args[0]) : 5.0;
        int turns = args.length > 1 ? Integer.parseInt(args[1]) : 20;
        int civs = args.length > 2 ? Integer.parseInt(args[2]) : 8;
        String size = args.length > 3 ? args[3] : "small";
        Log.INSTANCE.setBackend(new DesktopLogBackend());
        Display.INSTANCE.setPlatform(new DesktopDisplay());
        Fonts.INSTANCE.setFontImplementation(new DesktopFont());
        UncivFiles.Companion.setSaverLoader(new DesktopSaverLoader());

        Thread watchdog = new Thread(() -> {
            try {
                Thread.sleep((long) ((seconds * 12 + 1200) * 1000));
            } catch (InterruptedException ignored) {
                return;
            }
            for (String s : SCENES) {
                if (!reported.contains(s)) say("OJH noscene " + s + " the driver ran out of time before reaching it");
            }
            System.exit(3);
        });
        watchdog.setDaemon(true);
        watchdog.start();

        Lwjgl3ApplicationConfiguration config = new Lwjgl3ApplicationConfiguration();
        config.setTitle("OJH frame rate: Unciv");
        config.setWindowedMode(1600, 900);
        config.useVsync(false);
        config.setForegroundFPS(0);
        config.setIdleFPS(10000);
        new Lwjgl3Application(new Game(seconds, turns, civs, size), config);
    }
}
