const { Client, IntentsBitField, EmbedBuilder, ActionRowBuilder, ButtonBuilder, ButtonStyle, ModalBuilder, TextInputBuilder, TextInputStyle, PermissionsBitField } = require('discord.js');

const GUILD_ID = 'paste ur server id'; // Replace with your server ID
const MODMAIL_CATEGORY_NAME = 'ModMail'; // create category named ModMail or change it urself
const bot_token = 'paste token'; // Replace with your token

const intents = new IntentsBitField([
    IntentsBitField.Flags.Guilds,
    IntentsBitField.Flags.GuildMessages,
    IntentsBitField.Flags.DirectMessages,
    IntentsBitField.Flags.MessageContent,
    IntentsBitField.Flags.GuildMembers
]);

const bot = new Client({ intents });
const ticketStartTimes = new Map();

class CloseReasonModal extends ModalBuilder {
    constructor(user, staff, channel) {
        super();
        this.setTitle('Close ModMail Ticket');
        this.setCustomId('close_reason_modal');
        this.user = user;
        this.staff = staff;
        this.channel = channel;

        const reasonInput = new TextInputBuilder()
            .setCustomId('reason')
            .setLabel('Reason for closing')
            .setStyle(TextInputStyle.Paragraph);
        this.addComponents(new ActionRowBuilder().addComponents(reasonInput));
    }

    async handleSubmit(interaction) {
        const now = new Date();
        const openTime = ticketStartTimes.get(this.channel.id);
        const duration = openTime ? Math.floor((now - openTime) / 1000) : null;
        const formattedDuration = duration ? `${Math.floor(duration / 3600)}h ${Math.floor((duration % 3600) / 60)}m ${duration % 60}s` : 'Unknown';

        const embed = new EmbedBuilder()
            .setTitle('📫 ModMail Closed')
            .setColor(0xFF0000)
            .addFields(
                { name: 'Closed By', value: `${this.staff} (\`${this.staff.id}\`)`, inline: false },
                { name: 'Time Open', value: formattedDuration, inline: false },
                { name: 'Reason', value: interaction.fields.getTextInputValue('reason'), inline: false }
            )
            .setTimestamp(now);

        try {
            await this.user.send({ embeds: [embed] });
        } catch {
            await interaction.channel.send('⚠️ Could not notify the user.');
        }

        const logChannel = interaction.guild.channels.cache.find(ch => ch.name === 'modmail-log');
        if (logChannel) {
            const logEmbed = new EmbedBuilder(embed.data)
                .setTitle('🗑️ Ticket Closed')
                .setDescription(`Thread: ${this.channel} was closed.`);
            await logChannel.send({ embeds: [logEmbed] });
        }

        await interaction.reply({ content: '✅ Ticket closed.', ephemeral: true });
        await this.channel.delete();
    }
}

class CloseTicketView extends ActionRowBuilder {
    constructor(user, staff, channel) {
        super();
        this.user = user;
        this.staff = staff;
        this.channel = channel;

        const closeButton = new ButtonBuilder()
            .setCustomId('close_button')
            .setLabel('Close')
            .setStyle(ButtonStyle.Danger)
            .setEmoji('🗑️');
        this.addComponents(closeButton);
    }
}

bot.on('ready', async () => {
    console.log(`✅ Bot online as ${bot.user.tag}`);
});

bot.on('messageCreate', async message => {
    if (message.author.bot) return;

    if (message.channel.type === 'DM') {
        const guild = bot.guilds.cache.get(GUILD_ID);
        if (!guild) return;

        let category = guild.channels.cache.find(c => c.type === 'GUILD_CATEGORY' && c.name === MODMAIL_CATEGORY_NAME);
        if (!category) {
            category = await guild.channels.create({
                name: MODMAIL_CATEGORY_NAME,
                type: 'GUILD_CATEGORY'
            });
        }

        const channelName = `modmail-${message.author.id}`;
        let existingChannel = guild.channels.cache.find(ch => ch.name === channelName);

        if (!existingChannel) {
            const newChannel = await guild.channels.create({
                name: channelName,
                type: 'GUILD_TEXT',
                parent: category.id
            });

            const view = new CloseTicketView(message.author, bot.user, newChannel);
            await newChannel.send({
                content: `📬 New ModMail started by ${message.author}`,
                components: [view]
            });
            await newChannel.send(`**${message.author.tag}:** ${message.content}`);
            for (const attachment of message.attachments.values()) {
                await newChannel.send({ files: [attachment.url] });
            }
            await message.author.send('✅ Your message has been sent to the support team!');
            ticketStartTimes.set(newChannel.id, new Date());

            const logChannel = guild.channels.cache.find(ch => ch.name === 'modmail-log');
            if (logChannel) {
                const embed = new EmbedBuilder()
                    .setTitle('📉 New Ticket')
                    .setDescription(`${message.author} | \`${message.author.id}\` • ${new Date().toLocaleString()}`)
                    .setColor(0x5865F2);
                await logChannel.send({ embeds: [embed] });
            }
        } else {
            await existingChannel.send(`**${message.author.tag}:** ${message.content}`);
            for (const attachment of message.attachments.values()) {
                await existingChannel.send({ files: [attachment.url] });
            }
        }
    } else if (message.guild && message.channel.parent && message.channel.parent.name === MODMAIL_CATEGORY_NAME) {
        try {
            const userId = message.channel.name.replace('modmail-', '');
            const user = await bot.users.fetch(userId);
            if (message.content) {
                await user.send(message.content);
            }
            for (const attachment of message.attachments.values()) {
                await user.send({ files: [attachment.url] });
            }
        } catch {
            await message.channel.send('❌ Could not send message to user.');
        }
    }
});

bot.on('interactionCreate', async interaction => {
    if (!interaction.isButton() && !interaction.isModalSubmit()) return;

    if (interaction.customId === 'close_button') {
        if (!interaction.member.permissions.has(PermissionsBitField.Flags.ManageChannels)) {
            await interaction.reply({ content: 'You do not have permission to close this.', ephemeral: true });
            return;
        }
        const modal = new CloseReasonModal(interaction.user, interaction.user, interaction.channel);
        await interaction.showModal(modal);
    } else if (interaction.customId === 'close_reason_modal') {
        await new CloseReasonModal(interaction.user, interaction.user, interaction.channel).handleSubmit(interaction);
    }
});

bot.on('messageCreate', async message => {
    if (message.content.startsWith('!close')) {
        if (!message.channel.parent || message.channel.parent.name !== MODMAIL_CATEGORY_NAME) {
            await message.channel.send('❌ This command can only be used in a modmail thread.');
            return;
        }

        const args = message.content.split(' ').slice(1).join(' ');
        if (!args) {
            await message.channel.send('❌ You must provide a reason. Usage: `!close <reason>`');
            return;
        }

        try {
            await message.delete();
        } catch {}

        tryiogtry {
            const userId = message.channel.name.replace('modmail-', '');
            const user = await bot.users.fetch(userId);

            const startTime = ticketStartTimes.get(message.channel.id);
            const now = new Date();
            const duration = startTime ? Math.floor((now - startTime) / 1000) : null;
            const formattedDuration = duration ? `${Math.floor(duration / 3600)}h ${Math.floor((duration % 3600) / 60)}m ${duration % 60}s` : 'Unknown';

            const embed = new EmbedBuilder()
                .setTitle('📫 ModMail Closed')
                .setColor(0xFF0000)
                .addFields(
                    { name: 'Closed By', value: `${message.author} (\`${message.author.id}\`)`, inline: false },
                    { name: 'Time Open', value: formattedDuration, inline: false },
                    { name: 'Reason', value: args, inline: false }
                )
                .setTimestamp(now);

            await user.send({ embeds: [embed] });

            const logChannel = message.guild.channels.cache.find(ch => ch.name === 'modmail-log');
            if (logChannel) {
                const logEmbed = new EmbedBuilder(embed.data)
                    .setTitle('🗑️ Ticket Closed')
                    .setDescription(`Thread: ${message.channel} was closed.`);
                await logChannel.send({ embeds: [logEmbed] });
            }

            await message.channel.send('✅ Ticket closed.');
            await message.channel.delete();
        } catch {
            await message.channel.send('⚠️ Could not DM the user.');
        }
    }
});

bot.login(bot_token);
