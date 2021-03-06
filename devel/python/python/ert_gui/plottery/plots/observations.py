def plotObservations(plot_context, axes):
    ert = plot_context.ert()
    key = plot_context.key()
    config = plot_context.plotConfig()
    case_list = plot_context.cases()
    data_gatherer = plot_context.dataGatherer()

    if config.isObservationsEnabled() and data_gatherer.hasObservationGatherFunction():
        if len(case_list) > 0:
            observation_data = data_gatherer.gatherObservationData(ert, case_list[0], key)

            if not observation_data.empty:
                _plotObservations(axes, config, observation_data, value_column=key)



def _plotObservations(axes, plot_config, data, value_column):
    """
    @type axes: matplotlib.axes.Axes
    @type plot_config: PlotConfig
    @type data: DataFrame
    @type value_column: Str
    """

    error_color = plot_config.observationsColor()
    error_alpha = plot_config.observationsAlpha()

    errorbars = axes.errorbar(x=data.index.values, y=data[value_column], yerr=data["STD_%s" % value_column],
                 fmt=' ', ecolor=error_color, alpha=error_alpha)