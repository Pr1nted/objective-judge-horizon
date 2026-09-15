import java.lang.reflect.Field;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.LogRecord;
import java.util.logging.Logger;
import net.sf.freecol.FreeCol;
import net.sf.freecol.common.FreeColSeed;
import net.sf.freecol.common.i18n.Messages;
import net.sf.freecol.common.io.FreeColDirectories;
import net.sf.freecol.common.io.FreeColRules;
import net.sf.freecol.common.model.NationOptions;
import net.sf.freecol.common.model.Player;
import net.sf.freecol.common.model.Specification;
import net.sf.freecol.common.networking.ChangeSet;
import net.sf.freecol.common.option.IntegerOption;
import net.sf.freecol.common.option.MapGeneratorOptions;
import net.sf.freecol.server.FreeColServer;
import net.sf.freecol.server.control.InGameController;
import net.sf.freecol.server.model.ServerGame;
import net.sf.freecol.server.model.ServerPlayer;

public final class FreeColTpm {
    public static void main(String[] args) throws Exception {
        int turns = Integer.parseInt(args[0]);
        String seed = args[1];
        int europeans = Integer.parseInt(args[2]);
        int width = Integer.parseInt(args[3]);
        int height = Integer.parseInt(args[4]);

        final AtomicInteger aiErrors = new AtomicInteger();
        Logger freecol = Logger.getLogger("net.sf.freecol");
        freecol.setLevel(Level.WARNING);
        for (Handler handler : Logger.getLogger("").getHandlers()) handler.setLevel(Level.OFF);
        freecol.addHandler(new Handler() {
            @Override
            public void publish(LogRecord record) {
                if (record.getLevel() == Level.SEVERE) aiErrors.incrementAndGet();
            }

            @Override
            public void flush() {
            }

            @Override
            public void close() {
            }
        });

        FreeColSeed.setFreeColSeed(seed);
        FreeColDirectories.setDataDirectory("data");
        Messages.loadMessageBundle(Locale.US);
        FreeColDirectories.setUserDirectories();
        Field count = FreeCol.class.getDeclaredField("europeanCount");
        count.setAccessible(true);
        count.setInt(null, europeans);

        FreeColRules.loadRules();
        Specification spec = FreeColRules.getFreeColRulesFile("classic").getSpecification();
        spec.prepare(NationOptions.Advantages.SELECTABLE, "model.difficulty.medium");
        spec.getOption(MapGeneratorOptions.MAP_WIDTH, IntegerOption.class).setValue(width);
        spec.getOption(MapGeneratorOptions.MAP_HEIGHT, IntegerOption.class).setValue(height);

        FreeColServer server = new FreeColServer(false, true, spec, null, 0, "OJH");
        server.startGame();
        ServerGame game = server.getGame();
        InGameController control = server.getInGameController();
        Field onlyAI = InGameController.class.getDeclaredField("debugOnlyAITurns");
        onlyAI.setAccessible(true);
        onlyAI.setInt(control, turns + 1000);

        System.out.println("OJH players " + game.getLivePlayerList().size());
        System.out.println("OJH regions " + (width * height) + " tiles");
        System.out.println("OJH ready");
        System.out.flush();

        Player first = game.getFirstPlayer();
        game.setCurrentPlayer(first);
        int start = game.getTurn().getNumber();
        int last = start;
        ChangeSet opening = control.endTurn((ServerPlayer) first);
        if (opening != null) ((ServerPlayer) first).send(opening);

        while (last - start < turns) {
            Thread.sleep(1);
            int now = game.getTurn().getNumber();
            while (last < now && last - start < turns) {
                last++;
                System.out.println("OJH turn " + (last - start));
                System.out.flush();
            }
        }
        System.err.println("FreeCol AI errors: " + aiErrors.get() + ", live players at the end: " + game.getLivePlayerList().size());
        System.exit(aiErrors.get() == 0 ? 0 : 3);
    }
}
